
#-*- coding: utf-8 -*-
import subprocess
import os
import sys
import numpy as np
import sqlite3
import argparse

def array_to_blob(array):
    return array.tobytes()

def blob_to_array(blob, dtype, shape=(-1,)):
    return np.frombuffer(blob, dtype=dtype).reshape(*shape)

MAX_IMAGE_ID = 2**31 - 1

CREATE_CAMERAS_TABLE = """CREATE TABLE IF NOT EXISTS cameras (
    camera_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    model INTEGER NOT NULL,
    width INTEGER NOT NULL,
    height INTEGER NOT NULL,
    params BLOB,
    prior_focal_length INTEGER NOT NULL)"""

CREATE_DESCRIPTORS_TABLE = """CREATE TABLE IF NOT EXISTS descriptors (
    image_id INTEGER PRIMARY KEY NOT NULL,
    rows INTEGER NOT NULL,
    cols INTEGER NOT NULL,
    data BLOB,
    FOREIGN KEY(image_id) REFERENCES images(image_id) ON DELETE CASCADE)"""

CREATE_IMAGES_TABLE = """CREATE TABLE IF NOT EXISTS images (
    image_id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    name TEXT NOT NULL UNIQUE,
    camera_id INTEGER NOT NULL,
    CONSTRAINT image_id_check CHECK(image_id >= 0 and image_id < {}),
    FOREIGN KEY(camera_id) REFERENCES cameras(camera_id))
""".format(MAX_IMAGE_ID)

CREATE_POSE_PRIORS_TABLE = """CREATE TABLE IF NOT EXISTS pose_priors (
    image_id INTEGER PRIMARY KEY NOT NULL,
    position BLOB,
    coordinate_system INTEGER NOT NULL,
    position_covariance BLOB,
    FOREIGN KEY(image_id) REFERENCES images(image_id) ON DELETE CASCADE)"""

CREATE_TWO_VIEW_GEOMETRIES_TABLE = """
CREATE TABLE IF NOT EXISTS two_view_geometries (
    pair_id INTEGER PRIMARY KEY NOT NULL,
    rows INTEGER NOT NULL,
    cols INTEGER NOT NULL,
    data BLOB,
    config INTEGER NOT NULL,
    F BLOB,
    E BLOB,
    H BLOB,
    qvec BLOB,
    tvec BLOB)
"""

CREATE_KEYPOINTS_TABLE = """CREATE TABLE IF NOT EXISTS keypoints (
    image_id INTEGER PRIMARY KEY NOT NULL,
    rows INTEGER NOT NULL,
    cols INTEGER NOT NULL,
    data BLOB,
    FOREIGN KEY(image_id) REFERENCES images(image_id) ON DELETE CASCADE)
"""

CREATE_MATCHES_TABLE = """CREATE TABLE IF NOT EXISTS matches (
    pair_id INTEGER PRIMARY KEY NOT NULL,
    rows INTEGER NOT NULL,
    cols INTEGER NOT NULL,
    data BLOB)"""

CREATE_NAME_INDEX = (
    "CREATE UNIQUE INDEX IF NOT EXISTS index_name ON images(name)"
)

CREATE_ALL = "; ".join(
    [
        CREATE_CAMERAS_TABLE,
        CREATE_IMAGES_TABLE,
        CREATE_POSE_PRIORS_TABLE,
        CREATE_KEYPOINTS_TABLE,
        CREATE_DESCRIPTORS_TABLE,
        CREATE_MATCHES_TABLE,
        CREATE_TWO_VIEW_GEOMETRIES_TABLE,
        CREATE_NAME_INDEX,
    ]
)

class COLMAPDatabase(sqlite3.Connection):

    def connect(database_path):
        return sqlite3.connect(database_path, factory=COLMAPDatabase)

    def __init__(self):
        super(COLMAPDatabase, self).__init__()

        self.create_tables = lambda: self.executescript(CREATE_ALL)
        self.create_cameras_table = lambda: self.executescript(CREATE_CAMERAS_TABLE)
        self.create_descriptors_table = lambda: self.executescript(CREATE_DESCRIPTORS_TABLE)
        self.create_images_table = lambda: self.executescript(CREATE_IMAGES_TABLE)
        self.create_two_view_geometries_table = lambda: self.executescript(CREATE_TWO_VIEW_GEOMETRIES_TABLE)
        self.create_keypoints_table = lambda: self.executescript(CREATE_KEYPOINTS_TABLE)
        self.create_matches_table = lambda: self.executescript(CREATE_MATCHES_TABLE)
        self.create_name_index = lambda: self.executescript(CREATE_NAME_INDEX)

    def update_camera(self, model, width, height, params, camera_id):
        params = np.asarray(params, np.float64)
        cursor = self.execute(
            "UPDATE cameras SET model=?, width=?, height=?, params=?, prior_focal_length=False WHERE camera_id=?",
            (model, width, height, array_to_blob(params),camera_id))
        return cursor.lastrowid

class ColmapHelper:
    def __init__(self):
        parser = argparse.ArgumentParser(description='Gaussian Splatting Helper')
        parser.add_argument("workDir", help= "Work Directory");
        self.args = parser.parse_args()
        print(self.args)
        os.chdir(self.args.workDir)

    def runCommand(self, command):
        process = subprocess.Popen(command, encoding='utf-8', stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=self.args.workDir, shell=True, universal_newlines=True)
        while True:
            output = process.stdout.readline()
            if output == '' and process.poll() is not None:
                break
            if output:
                print(output.strip())
        if process.returncode!= 0:
            print(f"Command '{command}' failed with return code {process.returncode}", file=sys.stderr)

    def featureExtractor(self):
        self.runCommand("colmap feature_extractor --database_path database.db --image_path images")

    def commitCameraData(self):
        idList=list()
        modelList=list()
        widthList=list()
        heightList=list()
        paramsList=list()
        self.database = COLMAPDatabase.connect("database.db")
        with open(self.args.workDir + "/text/cameras.txt", "r") as cam:
            lines = cam.readlines()
            for i in range(0,len(lines),1):
                if lines[i][0]!='#':
                    strLists = lines[i].split()
                    cameraId=int(strLists[0])
                    cameraModel= 0
                    width=int(strLists[2])
                    height=int(strLists[3])
                    paramstr = np.array(strLists[4:])
                    params = paramstr.astype(np.float64)
                    idList.append(cameraId)
                    modelList.append(cameraModel)
                    widthList.append(width)
                    heightList.append(height)
                    paramsList.append(params)
                    camera_id = self.database.update_camera(cameraModel, width, height, params, cameraId)
        self.database.commit()
        rows = self.database.execute("SELECT * FROM cameras")
        for i in range(0,len(idList),1):
            camera_id, model, width, height, params, prior = next(rows)
            params = blob_to_array(params, np.float64)
            assert camera_id == idList[i]
            assert model == modelList[i] and width == widthList[i] and height == heightList[i]
            assert np.allclose(params, paramsList[i])

        # Close database.db.
        self.database.close()

    def exhaustiveMatcher(self):
        self.runCommand("colmap exhaustive_matcher --database_path database.db")
           
    def pointTriangulator(self):
        self.runCommand("colmap point_triangulator --database_path database.db --image_path images --input_path ./text --output_path ./pointTriangulator --Mapper.fix_existing_images 1 --Mapper.ba_refine_focal_length 0")

    def mapper(self):
        self.runCommand("colmap mapper --database_path database.db --image_path images --output_path ./sparse  --Mapper.fix_existing_images 1")

    def modelAligner(self):
        self.runCommand("colmap model_aligner --input_path ./sparse/0 --output_path ./sparse/0 --ref_images_path ./text/imageRefPos.txt --ref_is_gps 0 --alignment_type custom --alignment_max_error 3")

    def executeGaussianSplatting(self):
        self.runCommand('conda activate gaussian_splatting && python E:\\gaussian-splatting\\train.py -s. -m./output --iterations 7000')

if __name__ == "__main__":
    colmapHelper = ColmapHelper()
    # colmapHelper.featureExtractor()
    # colmapHelper.commitCameraData()
    # colmapHelper.exhaustiveMatcher()
    # # colmapHelper.pointTriangulator()
    # colmapHelper.mapper()
    # colmapHelper.modelAligner()
    # colmapHelper.executeGaussianSplatting()
