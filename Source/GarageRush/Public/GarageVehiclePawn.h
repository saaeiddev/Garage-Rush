#pragma once
#include "CoreMinimal.h"
#include "WheeledVehiclePawn.h"
#include "ChaosVehicleWheel.h"
#include "GarageVehiclePawn.generated.h"

UCLASS()
class GARAGERUSH_API UGarageFrontWheel : public UChaosVehicleWheel {
    GENERATED_BODY()
public: UGarageFrontWheel();
};
UCLASS()
class GARAGERUSH_API UGarageRearWheel : public UChaosVehicleWheel {
    GENERATED_BODY()
public: UGarageRearWheel();
};
struct FInputActionValue;
class UInputAction;
class UInputMappingContext;

// Requires a real four-wheel skeletal vehicle, PhysicsAsset, vehicle AnimBP,
// calibrated wheel classes, and model-specific camera sockets authored in UE.
UCLASS()
class GARAGERUSH_API AGarageVehiclePawn : public AWheeledVehiclePawn {
    GENERATED_BODY()
public:
    AGarageVehiclePawn(const FObjectInitializer& Initializer);
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void PawnClientRestart() override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") FName VehicleId = TEXT("hatch");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class USpringArmComponent> ChaseArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UCameraComponent> ChaseCamera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<class UCameraComponent> CockpitCamera;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float BaseTireFriction = 2.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float FrontBrakeTorque = 1800;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Simulation") float RearBrakeTorque = 1400;
    UFUNCTION(BlueprintCallable) void ApplyMechanicalState();
    UFUNCTION(BlueprintCallable) void ApplyControlMappings();
    UFUNCTION(BlueprintPure) float SpeedKmh() const;
    UFUNCTION(BlueprintPure) float EngineRpm() const;
    UFUNCTION(BlueprintPure) int32 CurrentGear() const;
    UFUNCTION(BlueprintImplementableEvent) void OnRecoveryRequested();
    UFUNCTION(BlueprintImplementableEvent) void OnPauseRequested();
private:
    uint64 AppliedRevision = 0;
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> Throttle;
    UPROPERTY() TObjectPtr<UInputAction> Brake;
    UPROPERTY() TObjectPtr<UInputAction> Steering;
    UPROPERTY() TObjectPtr<UInputAction> Handbrake;
    UPROPERTY() TObjectPtr<UInputAction> Camera;
    UPROPERTY() TObjectPtr<UInputAction> Recover;
    UPROPERTY() TObjectPtr<UInputAction> Pause;
    void SetThrottle(const FInputActionValue& Value);
    void SetBrake(const FInputActionValue& Value);
    void SetSteering(const FInputActionValue& Value);
    void SetHandbrake(const FInputActionValue& Value);
    void SwitchCamera();
    void ResetCar();
};
