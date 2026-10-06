#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GarageSessionSubsystem.h"
#include "GaragePartActor.generated.h"

// Artist-authored mesh/pivot/attachment is mandatory. No placeholder mesh is
// substituted for a serviceable part if the production mesh is missing.
UCLASS()
class GARAGERUSH_API AGaragePartActor : public AActor {
    GENERATED_BODY()
public:
    AGaragePartActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UStaticMeshComponent> PartMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UBoxComponent> ServiceMount;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Service") FName VehicleId = TEXT("hatch");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Service") FName PartId = TEXT("battery");
    UFUNCTION(BlueprintCallable) void SetFocused(bool Focused);
    UFUNCTION(BlueprintCallable) void RefreshFromSession();
    UFUNCTION(BlueprintImplementableEvent) void OnInteract(EGarageTool SelectedTool);
    UFUNCTION(BlueprintImplementableEvent) void OnToolAction(EGarageTool SelectedTool);
protected:
    virtual void BeginPlay() override;
    UFUNCTION() void SessionChanged(FGarageActionResult Result);
};
