#include "GarageVehiclePawn.h"
#include "GarageSessionSubsystem.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

UGarageFrontWheel::UGarageFrontWheel() {
    WheelRadius = 34; WheelWidth = 22; bAffectedBySteering = true; bAffectedByHandbrake = false;
    SpringRate = 60000; SuspensionDampingRatio = .8f; SuspensionMaxRaise = 8; SuspensionMaxDrop = 12;
}
UGarageRearWheel::UGarageRearWheel() {
    WheelRadius = 34; WheelWidth = 22; bAffectedBySteering = false; bAffectedByHandbrake = true;
    SpringRate = 60000; SuspensionDampingRatio = .8f; SuspensionMaxRaise = 8; SuspensionMaxDrop = 12;
}
AGarageVehiclePawn::AGarageVehiclePawn(const FObjectInitializer& Initializer) : Super(Initializer) {
    PrimaryActorTick.bCanEverTick = true;
    ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm")); ChaseArm->SetupAttachment(GetMesh());
    ChaseArm->TargetArmLength = 480; ChaseArm->SetRelativeLocation(FVector(0, 0, 140)); ChaseArm->SetRelativeRotation(FRotator(-12, 0, 0)); ChaseArm->bEnableCameraLag = true;
    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera")); ChaseCamera->SetupAttachment(ChaseArm);
    CockpitCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("CockpitCamera")); CockpitCamera->SetupAttachment(GetMesh()); CockpitCamera->SetRelativeLocation(FVector(0, -35, 125)); CockpitCamera->SetActive(false);
    auto* Movement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
    Movement->WheelSetups.SetNum(4);
    const FName Bones[] = {TEXT("Wheel_FL"), TEXT("Wheel_FR"), TEXT("Wheel_RL"), TEXT("Wheel_RR")};
    for (int32 I = 0; I < 4; ++I) { Movement->WheelSetups[I].BoneName = Bones[I]; Movement->WheelSetups[I].WheelClass = I < 2 ? UGarageFrontWheel::StaticClass() : UGarageRearWheel::StaticClass(); }
    Movement->EngineSetup.MaxTorque = 260; Movement->EngineSetup.MaxRPM = 6500;
    auto* Curve = Movement->EngineSetup.TorqueCurve.GetRichCurve();
    Curve->AddKey(0, .55f); Curve->AddKey(1500, .85f); Curve->AddKey(3000, 1); Curve->AddKey(5000, .95f); Curve->AddKey(6500, .65f);
    Movement->TransmissionSetup.bUseAutomaticGears = true;
    auto Axis = [this](const TCHAR* Name) { auto* A = CreateDefaultSubobject<UInputAction>(Name); A->ValueType = EInputActionValueType::Axis1D; return A; };
    Throttle = Axis(TEXT("Throttle")); Brake = Axis(TEXT("Brake")); Steering = Axis(TEXT("Steering"));
    Handbrake = CreateDefaultSubobject<UInputAction>(TEXT("Handbrake")); Camera = CreateDefaultSubobject<UInputAction>(TEXT("Camera")); Recover = CreateDefaultSubobject<UInputAction>(TEXT("Recover")); Pause = CreateDefaultSubobject<UInputAction>(TEXT("Pause"));
}
void AGarageVehiclePawn::PawnClientRestart() { Super::PawnClientRestart(); ApplyControlMappings(); ApplyMechanicalState(); }
void AGarageVehiclePawn::ApplyControlMappings() {
    const auto* PC = Cast<APlayerController>(GetController()); if (!PC || !PC->GetLocalPlayer()) return;
    auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (!Input || !S) return;
    if (Mapping) Input->RemoveMappingContext(Mapping); Mapping = NewObject<UInputMappingContext>(this);
    const auto& B = S->GetCoreState().settings.bindings;
    auto Key = [&B](const char* K) { return FKey(FName(UTF8_TO_TCHAR(B.at(K).c_str()))); };
    auto Map = [this](UInputAction* A, FKey K, bool Negative = false) { auto& E = Mapping->MapKey(A, K); if (Negative) E.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping)); };
    Map(Throttle, Key("forward")); Map(Throttle, EKeys::Gamepad_RightTriggerAxis);
    Map(Brake, Key("back")); Map(Brake, EKeys::Gamepad_LeftTriggerAxis);
    Map(Steering, Key("right")); Map(Steering, Key("left"), true); Map(Steering, EKeys::Gamepad_LeftX);
    Map(Handbrake, Key("handbrake")); Map(Handbrake, EKeys::Gamepad_FaceButton_Bottom);
    Map(Camera, Key("camera")); Map(Camera, EKeys::Gamepad_RightShoulder);
    Map(Recover, Key("recover")); Map(Recover, EKeys::Gamepad_FaceButton_Top);
    Map(Pause, Key("pause")); Map(Pause, EKeys::Gamepad_Special_Right); Input->AddMappingContext(Mapping, 1);
}
void AGarageVehiclePawn::SetupPlayerInputComponent(UInputComponent* Input) {
    Super::SetupPlayerInputComponent(Input);
    if (auto* E = Cast<UEnhancedInputComponent>(Input)) {
        E->BindAction(Throttle, ETriggerEvent::Triggered, this, &AGarageVehiclePawn::SetThrottle); E->BindAction(Throttle, ETriggerEvent::Completed, this, &AGarageVehiclePawn::SetThrottle);
        E->BindAction(Brake, ETriggerEvent::Triggered, this, &AGarageVehiclePawn::SetBrake); E->BindAction(Brake, ETriggerEvent::Completed, this, &AGarageVehiclePawn::SetBrake);
        E->BindAction(Steering, ETriggerEvent::Triggered, this, &AGarageVehiclePawn::SetSteering); E->BindAction(Steering, ETriggerEvent::Completed, this, &AGarageVehiclePawn::SetSteering);
        E->BindAction(Handbrake, ETriggerEvent::Triggered, this, &AGarageVehiclePawn::SetHandbrake); E->BindAction(Handbrake, ETriggerEvent::Completed, this, &AGarageVehiclePawn::SetHandbrake);
        E->BindAction(Camera, ETriggerEvent::Started, this, &AGarageVehiclePawn::SwitchCamera); E->BindAction(Recover, ETriggerEvent::Started, this, &AGarageVehiclePawn::ResetCar); E->BindAction(Pause, ETriggerEvent::Started, this, &AGarageVehiclePawn::OnPauseRequested);
    }
}
void AGarageVehiclePawn::ApplyMechanicalState() {
    const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (!S) return;
    const std::string Id = TCHAR_TO_UTF8(*VehicleId.ToString()); const auto Found = S->GetCoreState().vehicles.find(Id);
    auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
    if (Found == S->GetCoreState().vehicles.end() || !M || M->Wheels.Num() != 4) return;
    const auto& V = Found->second; const auto Data = gr::diagnose(S->GetCoreState(), Id);
    M->SetMaxEngineTorque(static_cast<float>(Data.torqueNm));
    const char* Corners[] = {"fl", "fr", "rl", "rr"};
    for (int32 I = 0; I < 4; ++I) {
        auto Condition = [&V](const std::string& P) { const auto& Slot = V.parts.at(P); return Slot.installed ? Slot.installed->health : 0.; };
        auto Upgrade = [&V](const std::string& P) { const auto& Slot = V.parts.at(P); const auto* SKU = Slot.installed ? gr::findSku(Slot.installed->sku) : nullptr; return SKU ? SKU->grade : 0; };
        const std::string C = Corners[I]; const double Tire = Condition("tire_" + C); const double Brakes = FMath::Min(Condition("pads_" + C), Condition("disc_" + C));
        M->SetWheelFrictionMultiplier(I, BaseTireFriction * static_cast<float>((.35 + .65 * Tire) * (1 + .1 * Upgrade("tire_" + C))));
        M->SetWheelMaxBrakeTorque(I, (I < 2 ? FrontBrakeTorque : RearBrakeTorque) * static_cast<float>((.3 + .7 * Brakes) * (1 + .05 * (Upgrade("pads_" + C) + Upgrade("disc_" + C)))));
        if (const auto* W = M->Wheels[I]) M->SetSuspensionParams(W->SpringRate, W->SuspensionDampingRatio * static_cast<float>((.3 + .7 * Condition("shock_" + C)) * (1 + .1 * Upgrade("shock_" + C))), W->SpringPreload, W->SuspensionMaxRaise, W->SuspensionMaxDrop, I);
    }
    AppliedRevision = V.revision;
}
void AGarageVehiclePawn::SetThrottle(const FInputActionValue& V) {
    const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); const std::string Id = TCHAR_TO_UTF8(*VehicleId.ToString());
    const auto& State = S->GetCoreState(); const auto Found = State.vehicles.find(Id);
    const bool Allowed = Found != State.vehicles.end() && Found->second.engineOn && !Found->second.lifted && gr::driveReady(State, Id);
    GetVehicleMovement()->SetThrottleInput(Allowed ? V.Get<float>() : 0);
}
void AGarageVehiclePawn::SetBrake(const FInputActionValue& V) { GetVehicleMovement()->SetBrakeInput(V.Get<float>()); }
void AGarageVehiclePawn::SetSteering(const FInputActionValue& V) { GetVehicleMovement()->SetSteeringInput(V.Get<float>()); }
void AGarageVehiclePawn::SetHandbrake(const FInputActionValue& V) { GetVehicleMovement()->SetHandbrakeInput(V.Get<bool>()); }
void AGarageVehiclePawn::SwitchCamera() { const bool Chase = ChaseCamera->IsActive(); ChaseCamera->SetActive(!Chase); CockpitCamera->SetActive(Chase); }
void AGarageVehiclePawn::ResetCar() { auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (S && S->Recover(VehicleId).Succeeded) OnRecoveryRequested(); }
float AGarageVehiclePawn::SpeedKmh() const { return GetVehicleMovement()->GetForwardSpeed() * .036f; }
float AGarageVehiclePawn::EngineRpm() const { const auto* M = Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement()); return M ? M->GetEngineRotationSpeed() : 0; }
int32 AGarageVehiclePawn::CurrentGear() const { return GetVehicleMovement()->GetCurrentGear(); }
void AGarageVehiclePawn::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds); const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (!S) return;
    const std::string Id = TCHAR_TO_UTF8(*VehicleId.ToString()); const auto Found = S->GetCoreState().vehicles.find(Id);
    if (Found != S->GetCoreState().vehicles.end()) {
        if (Found->second.revision != AppliedRevision) ApplyMechanicalState();
        if (!Found->second.engineOn || Found->second.lifted) GetVehicleMovement()->SetThrottleInput(0);
    }
}
