#include "GarageCharacter.h"
#include "GaragePartActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

AGarageCharacter::AGarageCharacter() {
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34, 90);
    GetCharacterMovement()->MaxWalkSpeed = 240;
    ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonView"));
    ViewCamera->SetupAttachment(GetCapsuleComponent()); ViewCamera->SetRelativeLocation(FVector(0, 0, 68)); ViewCamera->bUsePawnControlRotation = true;
    auto Axis = [this](const TCHAR* Name) { auto* Action = CreateDefaultSubobject<UInputAction>(Name); Action->ValueType = EInputActionValueType::Axis1D; return Action; };
    Forward = Axis(TEXT("MoveForward")); Right = Axis(TEXT("MoveRight")); LookX = Axis(TEXT("LookHorizontal")); LookY = Axis(TEXT("LookVertical"));
    Interact = CreateDefaultSubobject<UInputAction>(TEXT("Interact")); UseTool = CreateDefaultSubobject<UInputAction>(TEXT("Tool")); Board = CreateDefaultSubobject<UInputAction>(TEXT("Board")); Pause = CreateDefaultSubobject<UInputAction>(TEXT("Pause"));
}
void AGarageCharacter::PawnClientRestart() { Super::PawnClientRestart(); ApplyControlMappings(); }
void AGarageCharacter::ApplyControlMappings() {
    auto* PC = Cast<APlayerController>(GetController()); if (!PC || !PC->GetLocalPlayer()) return;
    auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
    auto* Session = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (!Input || !Session) return;
    if (Mapping) Input->RemoveMappingContext(Mapping);
    Mapping = NewObject<UInputMappingContext>(this);
    const auto& Bindings = Session->GetCoreState().settings.bindings;
    auto Key = [&Bindings](const char* Action) { return FKey(FName(UTF8_TO_TCHAR(Bindings.at(Action).c_str()))); };
    auto Map = [this](UInputAction* Action, FKey K, bool Negate = false) {
        auto& Entry = Mapping->MapKey(Action, K);
        if (Negate) Entry.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
    };
    Map(Forward, Key("forward")); Map(Forward, Key("back"), true); Map(Forward, EKeys::Gamepad_LeftY);
    Map(Right, Key("right")); Map(Right, Key("left"), true); Map(Right, EKeys::Gamepad_LeftX);
    Map(LookX, EKeys::MouseX); Map(LookX, EKeys::Gamepad_RightX);
    Map(LookY, EKeys::MouseY); Map(LookY, EKeys::Gamepad_RightY);
    Map(Interact, Key("interact")); Map(Interact, EKeys::Gamepad_FaceButton_Bottom);
    Map(UseTool, Key("tool")); Map(UseTool, EKeys::Gamepad_RightTrigger);
    Map(Board, Key("board")); Map(Board, EKeys::Gamepad_Special_Left);
    Map(Pause, Key("pause")); Map(Pause, EKeys::Gamepad_Special_Right);
    Input->AddMappingContext(Mapping, 0);
}
void AGarageCharacter::SetupPlayerInputComponent(UInputComponent* Input) {
    Super::SetupPlayerInputComponent(Input);
    if (auto* Enhanced = Cast<UEnhancedInputComponent>(Input)) {
        Enhanced->BindAction(Forward, ETriggerEvent::Triggered, this, &AGarageCharacter::MoveForward);
        Enhanced->BindAction(Right, ETriggerEvent::Triggered, this, &AGarageCharacter::MoveRight);
        Enhanced->BindAction(LookX, ETriggerEvent::Triggered, this, &AGarageCharacter::Turn);
        Enhanced->BindAction(LookY, ETriggerEvent::Triggered, this, &AGarageCharacter::Look);
        Enhanced->BindAction(Interact, ETriggerEvent::Started, this, &AGarageCharacter::DoInteract);
        Enhanced->BindAction(UseTool, ETriggerEvent::Started, this, &AGarageCharacter::DoTool);
        Enhanced->BindAction(Board, ETriggerEvent::Started, this, &AGarageCharacter::OnJobBoardRequested);
        Enhanced->BindAction(Pause, ETriggerEvent::Started, this, &AGarageCharacter::OnPauseRequested);
    }
}
void AGarageCharacter::MoveForward(const FInputActionValue& V) { if (Controller) AddMovementInput(FRotator(0, Controller->GetControlRotation().Yaw, 0).Vector(), V.Get<float>()); }
void AGarageCharacter::MoveRight(const FInputActionValue& V) { if (Controller) AddMovementInput(FRotationMatrix(FRotator(0, Controller->GetControlRotation().Yaw, 0)).GetUnitAxis(EAxis::Y), V.Get<float>()); }
void AGarageCharacter::Turn(const FInputActionValue& V) { const auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); AddControllerYawInput(V.Get<float>() * static_cast<float>(S->GetCoreState().settings.sensitivity)); }
void AGarageCharacter::Look(const FInputActionValue& V) { const auto& P = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>()->GetCoreState().settings; AddControllerPitchInput(V.Get<float>() * static_cast<float>(P.sensitivity) * (P.invertLook ? 1 : -1)); }
void AGarageCharacter::DoInteract() { if (FocusedPart) FocusedPart->OnInteract(SelectedTool); }
void AGarageCharacter::DoTool() { if (FocusedPart) FocusedPart->OnToolAction(SelectedTool); }
void AGarageCharacter::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds); if (!IsLocallyControlled()) return;
    auto* S = GetGameInstance()->GetSubsystem<UGarageSessionSubsystem>(); if (!S) return;
    ViewCamera->SetFieldOfView(static_cast<float>(S->GetCoreState().settings.fov)); S->RecordPlayerPosition(GetActorLocation());
    const FVector Start = ViewCamera->GetComponentLocation(); FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GaragePartFocus), true, this);
    GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + ViewCamera->GetForwardVector() * 230, ECC_Visibility, Params);
    auto* Next = Cast<AGaragePartActor>(Hit.GetActor());
    if (Next != FocusedPart) { if (FocusedPart) FocusedPart->SetFocused(false); FocusedPart = Next; }
    if (FocusedPart) FocusedPart->SetFocused(true);
}
