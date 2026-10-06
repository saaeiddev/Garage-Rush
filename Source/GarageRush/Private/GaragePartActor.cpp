#include "GaragePartActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/GameInstance.h"

AGaragePartActor::AGaragePartActor() {
    PartMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PartMesh")); SetRootComponent(PartMesh);
    PartMesh->SetCollisionProfileName(TEXT("BlockAllDynamic")); PartMesh->SetCustomDepthStencilValue(1);
    ServiceMount = CreateDefaultSubobject<UBoxComponent>(TEXT("ServiceMount")); ServiceMount->SetupAttachment(PartMesh);
    ServiceMount->SetBoxExtent(FVector(8)); ServiceMount->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ServiceMount->SetCollisionResponseToAllChannels(ECR_Ignore); ServiceMount->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}
void AGaragePartActor::BeginPlay() {
    Super::BeginPlay();
    if (auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>()) S->OnSessionChanged.AddDynamic(this, &AGaragePartActor::SessionChanged);
    RefreshFromSession();
}
void AGaragePartActor::SetFocused(bool Focused) {
    const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>();
    PartMesh->SetRenderCustomDepth(Focused && S && S->GetCoreState().settings.assisted);
}
void AGaragePartActor::SessionChanged(FGarageActionResult Result) { if (Result.Succeeded) RefreshFromSession(); }
void AGaragePartActor::RefreshFromSession() {
    const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (!S) return;
    for (const auto& P : S->GetParts(VehicleId)) if (P.Id == PartId) {
        PartMesh->SetVisibility(P.Installed); PartMesh->SetCollisionEnabled(P.Installed ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        SetActorHiddenInGame(false); SetActorEnableCollision(true); return;
    }
    // Invalid authored IDs fail visibly in the Editor validation tool; hide the
    // actor at runtime so an unknown part can never be picked or transacted.
    SetActorHiddenInGame(true); SetActorEnableCollision(false);
}
