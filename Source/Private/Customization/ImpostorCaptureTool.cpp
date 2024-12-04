//#include "ImpostorCaptureTool.h"
//#include "Kismet/KismetRenderingLibrary.h"
//#include "Kismet/KismetMathLibrary.h"
//
//TSharedPtr<SWidget> UImpostorCaptureTool::BuildWidget()
//{
//	return SNew(SSpacer);
//}
//
//void UImpostorCaptureTool::SetupRTAndSaveList()
//{
//	UKismetMathLibrary::Log(Resolution,1.99999f);
//}
//
//void UImpostorCaptureTool::ClearRenderTargets()
//{
//	UKismetRenderingLibrary::ClearRenderTarget2D(this, CombinedAlphasRT, FLinearColor::Black);
//	UKismetRenderingLibrary::ClearRenderTarget2D(this, ScratchRT, FLinearColor::Black);
//	for (auto& Item : SceneCaptureMipChain) {
//		UKismetRenderingLibrary::ClearRenderTarget2D(this, Item, FLinearColor::Black);
//	}
//	for (auto& Item : TargetChannelsMap) {
//		UKismetRenderingLibrary::ClearRenderTarget2D(this, Item.Value, FLinearColor::Black);
//	}
//}
//
