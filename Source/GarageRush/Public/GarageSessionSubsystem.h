#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/GarageCore.h"
#include "GarageSessionSubsystem.generated.h"

UENUM(BlueprintType)
enum class EGarageTool : uint8 { Hand, Socket, TorqueWrench, Multimeter, Scanner };
UENUM(BlueprintType)
enum class EGarageLocation : uint8 { Bay, Yard, Track };

USTRUCT(BlueprintType)
struct FGarageActionResult {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool Succeeded = false;
    UPROPERTY(BlueprintReadOnly) FText Message;
    UPROPERTY(BlueprintReadOnly) int64 ItemSerial = 0;
};
USTRUCT(BlueprintType)
struct FGaragePartInfo {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    UPROPERTY(BlueprintReadOnly) FText Label;
    UPROPERTY(BlueprintReadOnly) bool Installed = false;
    UPROPERTY(BlueprintReadOnly) float ConditionPercent = 0;
    UPROPERTY(BlueprintReadOnly) int32 SecuredFasteners = 0;
    UPROPERTY(BlueprintReadOnly) int32 TotalFasteners = 0;
    UPROPERTY(BlueprintReadOnly) int64 Serial = 0;
    UPROPERTY(BlueprintReadOnly) FName Sku;
    UPROPERTY(BlueprintReadOnly) EGarageTool RemovalTool = EGarageTool::Hand;
    UPROPERTY(BlueprintReadOnly) bool RequiresHood = false;
    UPROPERTY(BlueprintReadOnly) bool RequiresLift = false;
    UPROPERTY(BlueprintReadOnly) TArray<FName> RemoveFirst;
};
USTRUCT(BlueprintType)
struct FGarageShopItem {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Sku;
    UPROPERTY(BlueprintReadOnly) FName Vehicle;
    UPROPERTY(BlueprintReadOnly) FText Label;
    UPROPERTY(BlueprintReadOnly) int64 PriceCents = 0;
    UPROPERTY(BlueprintReadOnly) int32 Stock = 0;
    UPROPERTY(BlueprintReadOnly) int32 Grade = 0;
};
USTRUCT(BlueprintType)
struct FGarageInventoryItem {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int64 Serial = 0;
    UPROPERTY(BlueprintReadOnly) FName Sku;
    UPROPERTY(BlueprintReadOnly) FName FitsVehicle;
    UPROPERTY(BlueprintReadOnly) FText Label;
    UPROPERTY(BlueprintReadOnly) float ConditionPercent = 0;
    UPROPERTY(BlueprintReadOnly) bool Refundable = false;
};
USTRUCT(BlueprintType)
struct FGarageOrderInfo {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName Id;
    UPROPERTY(BlueprintReadOnly) FName Vehicle;
    UPROPERTY(BlueprintReadOnly) FText Title;
    UPROPERTY(BlueprintReadOnly) FText Symptoms;
    UPROPERTY(BlueprintReadOnly) int64 PaymentCents = 0;
    UPROPERTY(BlueprintReadOnly) int32 Reputation = 0;
    UPROPERTY(BlueprintReadOnly) int64 CompletedCount = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FName> RequiredInspections;
    UPROPERTY(BlueprintReadOnly) int32 VerificationProtocol = 0;
};
USTRUCT(BlueprintType)
struct FGarageDiagnostics {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool Assembled = false;
    UPROPERTY(BlueprintReadOnly) bool Starts = false;
    UPROPERTY(BlueprintReadOnly) float BatteryVolts = 0;
    UPROPERTY(BlueprintReadOnly) float ChargingVolts = 0;
    UPROPERTY(BlueprintReadOnly) float CoolantC = 0;
    UPROPERTY(BlueprintReadOnly) float MisfirePercent = 0;
    UPROPERTY(BlueprintReadOnly) float IntakePercent = 0;
    UPROPERTY(BlueprintReadOnly) float MassKg = 0;
    UPROPERTY(BlueprintReadOnly) float PowerKw = 0;
    UPROPERTY(BlueprintReadOnly) float Grip = 0;
    UPROPERTY(BlueprintReadOnly) float Damping = 0;
    UPROPERTY(BlueprintReadOnly) float ReferenceZeroTo100 = 0;
    UPROPERTY(BlueprintReadOnly) float ReferenceBrake100Meters = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FText> Faults;
};
USTRUCT(BlueprintType)
struct FGaragePreferences {
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Preset = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fov = 85;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Sensitivity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TextScale = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool InvertLook = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Assisted = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool MotionBlur = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool FilmGrain = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool DepthOfField = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool CameraShake = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<float> Volumes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> Bindings;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGarageSessionChanged, FGarageActionResult, Result);

// Source integration, not yet compiled by UHT/UBT in this environment.
UCLASS()
class GARAGERUSH_API UGarageSessionSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UPROPERTY(BlueprintAssignable) FGarageSessionChanged OnSessionChanged;
    UPROPERTY(BlueprintReadOnly) FString LastSaveError;
    UFUNCTION(BlueprintCallable) FGarageActionResult NewSession(bool Sandbox);
    UFUNCTION(BlueprintCallable) FGarageActionResult SaveSession();
    UFUNCTION(BlueprintCallable) FGarageActionResult LoadSession(bool Sandbox);
    UFUNCTION(BlueprintPure) bool HasSave(bool Sandbox) const;
    UFUNCTION(BlueprintPure) int64 GetFundsCents() const;
    UFUNCTION(BlueprintPure) int32 GetReputation() const;
    UFUNCTION(BlueprintPure) FName GetActiveOrder() const;
    UFUNCTION(BlueprintPure) TArray<FGarageOrderInfo> GetOrders() const;
    UFUNCTION(BlueprintPure) TArray<FGarageShopItem> GetShop() const;
    UFUNCTION(BlueprintPure) TArray<FGarageInventoryItem> GetInventory() const;
    UFUNCTION(BlueprintPure) TArray<FGaragePartInfo> GetParts(FName Vehicle) const;
    UFUNCTION(BlueprintPure) FGarageDiagnostics GetDiagnostics(FName Vehicle) const;
    UFUNCTION(BlueprintPure) FGaragePreferences GetPreferences() const;
    UFUNCTION(BlueprintCallable) FGarageActionResult SetPreferences(const FGaragePreferences& Preferences);
    UFUNCTION(BlueprintCallable) FGarageActionResult Accept(FName Order);
    UFUNCTION(BlueprintCallable) FGarageActionResult Inspect(FName Vehicle, FName Part, EGarageTool Tool);
    UFUNCTION(BlueprintCallable) FGarageActionResult Buy(FName Sku);
    UFUNCTION(BlueprintCallable) FGarageActionResult Refund(int64 Serial);
    UFUNCTION(BlueprintCallable) FGarageActionResult Hood(FName Vehicle, bool Open);
    UFUNCTION(BlueprintCallable) FGarageActionResult Lift(FName Vehicle, bool Up);
    UFUNCTION(BlueprintCallable) FGarageActionResult Fastener(FName Vehicle, FName Part, EGarageTool Tool, bool Tighten);
    UFUNCTION(BlueprintCallable) FGarageActionResult Remove(FName Vehicle, FName Part, EGarageTool Tool);
    UFUNCTION(BlueprintCallable) FGarageActionResult Install(FName Vehicle, FName Part, int64 Serial);
    UFUNCTION(BlueprintCallable) FGarageActionResult Move(FName Vehicle, EGarageLocation Location);
    UFUNCTION(BlueprintCallable) FGarageActionResult Engine(FName Vehicle, bool On);
    UFUNCTION(BlueprintCallable) FGarageActionResult Recover(FName Vehicle);
    UFUNCTION(BlueprintCallable) FGarageActionResult VerifyCoreAcceptance();
    UFUNCTION(BlueprintCallable) FGarageActionResult CollectPayment();
    const gr::State& GetCoreState() const { return Session; }
    void RecordPlayerPosition(const FVector& Position);
private:
    gr::State Session;
    bool HasStarted = false;
    FString SlotPath(bool Sandbox) const;
    FGarageActionResult Finish(const gr::Result& Result, bool Persist = true);
};
