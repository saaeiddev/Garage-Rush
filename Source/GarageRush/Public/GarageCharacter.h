#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GarageSessionSubsystem.h"
#include "GarageCharacter.generated.h"

struct FInputActionValue;
class UInputAction;
class UInputMappingContext;
class UCameraComponent;
class AGaragePartActor;

UCLASS()
class GARAGERUSH_API AGarageCharacter : public ACharacter {
    GENERATED_BODY()
public:
    AGarageCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void PawnClientRestart() override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> ViewCamera;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGarageTool SelectedTool = EGarageTool::Hand;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AGaragePartActor> FocusedPart;
    UFUNCTION(BlueprintCallable) void ApplyControlMappings();
    UFUNCTION(BlueprintImplementableEvent) void OnJobBoardRequested();
    UFUNCTION(BlueprintImplementableEvent) void OnPauseRequested();
private:
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> Forward;
    UPROPERTY() TObjectPtr<UInputAction> Right;
    UPROPERTY() TObjectPtr<UInputAction> LookX;
    UPROPERTY() TObjectPtr<UInputAction> LookY;
    UPROPERTY() TObjectPtr<UInputAction> Interact;
    UPROPERTY() TObjectPtr<UInputAction> UseTool;
    UPROPERTY() TObjectPtr<UInputAction> Board;
    UPROPERTY() TObjectPtr<UInputAction> Pause;
    void MoveForward(const FInputActionValue& Value);
    void MoveRight(const FInputActionValue& Value);
    void Turn(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void DoInteract();
    void DoTool();
};
