#include "HLODBuilderImpostor.h"
#include "Serialization/ArchiveCrc32.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Kismet/GameplayStatics.h"


UHLODBuilderImpostorSettings::UHLODBuilderImpostorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

uint32 UHLODBuilderImpostorSettings::GetCRC() const
{
	FArchiveCrc32 Ar;
	FString HLODBaseKey = "1EC5FBC75A71412EB296F0E7E8424814";
	Ar << HLODBaseKey;
	uint32 Hash = Ar.GetCrc();
	return Hash;
}

UHLODBuilderImpostor::UHLODBuilderImpostor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


TArray<UActorComponent*> UHLODBuilderImpostor::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	//UWorld* World = InHLODBuildContext.World;
	//if (!BP_Generate_ImposterSpritesActor) {
	//	if (BP_Generate_ImposterSprites) {
	//		BP_Generate_ImposterSpritesActor = World->SpawnActor(BP_Generate_ImposterSprites);
	//	}
	//	else {
	//		FNotificationInfo Info(FText::FromString(TEXT("ERROR! Please enable ImpostorBaker plugin")));
	//		Info.FadeInDuration = 2.0f;
	//		Info.ExpireDuration = 2.0f;
	//		Info.FadeOutDuration = 2.0f;
	//		FSlateNotificationManager::Get().AddNotification(Info);
	//	}
	//}
	return Super::Build(InHLODBuildContext, InSourceComponents);
}