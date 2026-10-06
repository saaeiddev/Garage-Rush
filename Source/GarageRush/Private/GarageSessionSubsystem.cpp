#include "GarageSessionSubsystem.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Engine/GameInstance.h"

namespace {
std::string Id(FName Name) { return TCHAR_TO_UTF8(*Name.ToString()); }
FName Name(const std::string& Text) { return FName(UTF8_TO_TCHAR(Text.c_str())); }
FText Localized(const std::string& Key, const std::string& Text) {
    return FText::ChangeKey(TEXT("GarageRush"), UTF8_TO_TCHAR(Key.c_str()), FText::FromString(UTF8_TO_TCHAR(Text.c_str())));
}
std::filesystem::path NativePath(const FString& Path) {
#if PLATFORM_WINDOWS
    return std::filesystem::path(*Path);
#else
    return std::filesystem::path(TCHAR_TO_UTF8(*Path));
#endif
}
}
void UGarageSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection); Session = gr::newGame();
}
FString UGarageSessionSubsystem::SlotPath(bool Sandbox) const {
    return FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("GarageRush"), TEXT("Saved"), Sandbox ? TEXT("Sandbox.grsave") : TEXT("Career.grsave"));
}
FGarageActionResult UGarageSessionSubsystem::Finish(const gr::Result& Result, bool Persist) {
    FGarageActionResult Out; Out.Succeeded = Result.ok; Out.ItemSerial = static_cast<int64>(Result.item);
    Out.Message = Localized("feedback." + Result.message, Result.message);
    if (Result.ok && Persist && HasStarted) {
        const auto Saved = gr::save(Session, NativePath(SlotPath(Session.mode == gr::Mode::Sandbox)));
        LastSaveError = Saved.ok ? FString() : FString(UTF8_TO_TCHAR(Saved.message.c_str()));
    }
    OnSessionChanged.Broadcast(Out); return Out;
}
FGarageActionResult UGarageSessionSubsystem::NewSession(bool Sandbox) {
    Session = gr::newGame(Sandbox ? gr::Mode::Sandbox : gr::Mode::Career); HasStarted = true;
    return Finish({true, "New session started.", 0});
}
FGarageActionResult UGarageSessionSubsystem::SaveSession() {
    if (!HasStarted) return Finish({false, "Start or continue a session first.", 0}, false);
    const auto Result = gr::save(Session, NativePath(SlotPath(Session.mode == gr::Mode::Sandbox)));
    LastSaveError = Result.ok ? FString() : FString(UTF8_TO_TCHAR(Result.message.c_str()));
    return Finish(Result, false);
}
FGarageActionResult UGarageSessionSubsystem::LoadSession(bool Sandbox) {
    gr::State Loaded;
    const auto Result = gr::load(Loaded, NativePath(SlotPath(Sandbox)));
    if (!Result.result) return Finish(Result.result, false);
    if ((Loaded.mode == gr::Mode::Sandbox) != Sandbox) return Finish({false, "Save mode does not match this slot.", 0}, false);
    Session = std::move(Loaded); HasStarted = true; LastSaveError.Empty(); return Finish(Result.result, false);
}
bool UGarageSessionSubsystem::HasSave(bool Sandbox) const {
    const auto Path = SlotPath(Sandbox);
    return IFileManager::Get().FileExists(*Path) || IFileManager::Get().FileExists(*(Path + TEXT(".bak")));
}
int64 UGarageSessionSubsystem::GetFundsCents() const { return Session.money; }
int32 UGarageSessionSubsystem::GetReputation() const { return Session.reputation; }
FName UGarageSessionSubsystem::GetActiveOrder() const { return Session.job ? Name(Session.job->order) : NAME_None; }
TArray<FGarageOrderInfo> UGarageSessionSubsystem::GetOrders() const {
    TArray<FGarageOrderInfo> Result;
    for (const auto& J : gr::orderCatalog()) {
        FGarageOrderInfo Info; Info.Id = Name(J.id); Info.Vehicle = Name(J.vehicle);
        Info.Title = Localized("order." + J.id + ".title", J.title); Info.Symptoms = Localized("order." + J.id + ".symptoms", J.customerSymptoms);
        Info.PaymentCents = J.payment; Info.Reputation = J.reputation; Info.CompletedCount = Session.completions.at(J.id);
        Info.VerificationProtocol = static_cast<int32>(J.test);
        for (const auto& F : J.faults) Info.RequiredInspections.Add(Name(F.part));
        Result.Add(Info);
    } return Result;
}
TArray<FGarageShopItem> UGarageSessionSubsystem::GetShop() const {
    TArray<FGarageShopItem> Result;
    for (const auto& S : gr::shopCatalog()) {
        FGarageShopItem Info; Info.Sku = Name(S.id); Info.Vehicle = Name(S.vehicle); Info.Label = Localized("sku." + S.id, S.label);
        Info.PriceCents = S.price; Info.Stock = Session.stock.at(S.id); Info.Grade = S.grade; Result.Add(Info);
    } return Result;
}
TArray<FGarageInventoryItem> UGarageSessionSubsystem::GetInventory() const {
    TArray<FGarageInventoryItem> Result;
    for (const auto& Pair : Session.inventory) {
        const auto& Item = Pair.second; const auto* S = gr::findSku(Item.sku);
        FGarageInventoryItem Info; Info.Serial = static_cast<int64>(Item.serial); Info.Sku = Name(Item.sku); Info.FitsVehicle = Name(S->vehicle);
        Info.Label = Localized("sku." + S->id, S->label); Info.ConditionPercent = static_cast<float>(Item.health * 100); Info.Refundable = Item.refundable;
        Result.Add(Info);
    } return Result;
}
TArray<FGaragePartInfo> UGarageSessionSubsystem::GetParts(FName Vehicle) const {
    TArray<FGaragePartInfo> Result; const auto V = Id(Vehicle); const auto* Definition = gr::findVehicle(V);
    if (!Definition) return Result;
    const auto& State = Session.vehicles.at(V);
    for (const auto& P : Definition->parts) {
        const auto& Slot = State.parts.at(P.id); FGaragePartInfo Info;
        Info.Id = Name(P.id); Info.Label = Localized("part." + V + "." + P.id, P.label); Info.Installed = Slot.installed.has_value();
        Info.ConditionPercent = Slot.installed ? static_cast<float>(Slot.installed->health * 100) : 0;
        Info.SecuredFasteners = Slot.secured; Info.TotalFasteners = P.fasteners;
        Info.Serial = Slot.installed ? static_cast<int64>(Slot.installed->serial) : 0; Info.Sku = Slot.installed ? Name(Slot.installed->sku) : NAME_None;
        Info.RemovalTool = static_cast<EGarageTool>(P.tool); Info.RequiresHood = P.hood; Info.RequiresLift = P.lift;
        for (const auto& Dependency : P.removeFirst) Info.RemoveFirst.Add(Name(Dependency)); Result.Add(Info);
    } return Result;
}
FGarageDiagnostics UGarageSessionSubsystem::GetDiagnostics(FName Vehicle) const {
    const auto M = gr::diagnose(Session, Id(Vehicle)); FGarageDiagnostics D;
    D.Assembled = M.assembled; D.Starts = M.starts; D.BatteryVolts = static_cast<float>(M.batteryVolts); D.ChargingVolts = static_cast<float>(M.chargingVolts);
    D.CoolantC = static_cast<float>(M.coolantC); D.MisfirePercent = static_cast<float>(M.misfirePercent); D.IntakePercent = static_cast<float>(M.intakePercent);
    D.MassKg = static_cast<float>(M.massKg); D.PowerKw = static_cast<float>(M.powerKw); D.Grip = static_cast<float>(M.grip); D.Damping = static_cast<float>(M.damping);
    D.ReferenceZeroTo100 = static_cast<float>(M.zeroTo100); D.ReferenceBrake100Meters = static_cast<float>(M.brake100Meters);
    for (const auto& F : M.faults) D.Faults.Add(Localized("fault." + F, F)); return D;
}
FGaragePreferences UGarageSessionSubsystem::GetPreferences() const {
    const auto& P = Session.settings; FGaragePreferences Out;
    Out.Preset = P.preset; Out.Fov = static_cast<float>(P.fov); Out.Sensitivity = static_cast<float>(P.sensitivity); Out.TextScale = static_cast<float>(P.textScale);
    Out.InvertLook = P.invertLook; Out.Assisted = P.assisted; Out.MotionBlur = P.motionBlur; Out.FilmGrain = P.filmGrain; Out.DepthOfField = P.depthOfField; Out.CameraShake = P.cameraShake;
    for (double V : P.volumes) Out.Volumes.Add(static_cast<float>(V));
    for (const auto& B : P.bindings) Out.Bindings.Add(Name(B.first), Name(B.second)); return Out;
}
FGarageActionResult UGarageSessionSubsystem::SetPreferences(const FGaragePreferences& P) {
    if (P.Volumes.Num() != 5) return Finish({false, "Supply all five audio volumes.", 0}, false);
    gr::Settings Settings; Settings.preset = P.Preset; Settings.fov = P.Fov; Settings.sensitivity = P.Sensitivity; Settings.textScale = P.TextScale;
    Settings.invertLook = P.InvertLook; Settings.assisted = P.Assisted; Settings.motionBlur = P.MotionBlur; Settings.filmGrain = P.FilmGrain; Settings.depthOfField = P.DepthOfField; Settings.cameraShake = P.CameraShake;
    for (int32 I = 0; I < 5; ++I) Settings.volumes[static_cast<std::size_t>(I)] = P.Volumes[I];
    Settings.bindings.clear(); for (const auto& B : P.Bindings) Settings.bindings.emplace(Id(B.Key), Id(B.Value));
    return Finish(gr::updateSettings(Session, Settings));
}
FGarageActionResult UGarageSessionSubsystem::Accept(FName Order) { return Finish(gr::acceptOrder(Session, Id(Order))); }
FGarageActionResult UGarageSessionSubsystem::Inspect(FName V, FName P, EGarageTool T) { return Finish(gr::inspect(Session, Id(V), Id(P), static_cast<gr::Tool>(T))); }
FGarageActionResult UGarageSessionSubsystem::Buy(FName S) { return Finish(gr::purchase(Session, Id(S))); }
FGarageActionResult UGarageSessionSubsystem::Refund(int64 S) { return Finish(gr::refund(Session, static_cast<std::uint64_t>(S))); }
FGarageActionResult UGarageSessionSubsystem::Hood(FName V, bool Open) { return Finish(gr::setHood(Session, Id(V), Open)); }
FGarageActionResult UGarageSessionSubsystem::Lift(FName V, bool Up) { return Finish(gr::setLift(Session, Id(V), Up)); }
FGarageActionResult UGarageSessionSubsystem::Fastener(FName V, FName P, EGarageTool T, bool Tighten) { return Finish(gr::fastener(Session, Id(V), Id(P), static_cast<gr::Tool>(T), Tighten)); }
FGarageActionResult UGarageSessionSubsystem::Remove(FName V, FName P, EGarageTool T) { return Finish(gr::removePart(Session, Id(V), Id(P), static_cast<gr::Tool>(T))); }
FGarageActionResult UGarageSessionSubsystem::Install(FName V, FName P, int64 S) { return Finish(gr::installPart(Session, Id(V), Id(P), static_cast<std::uint64_t>(S))); }
FGarageActionResult UGarageSessionSubsystem::Move(FName V, EGarageLocation L) { return Finish(gr::moveVehicle(Session, Id(V), static_cast<gr::Location>(L))); }
FGarageActionResult UGarageSessionSubsystem::Engine(FName V, bool On) { return Finish(gr::setEngine(Session, Id(V), On)); }
FGarageActionResult UGarageSessionSubsystem::Recover(FName V) { return Finish(gr::recoverVehicle(Session, Id(V))); }
FGarageActionResult UGarageSessionSubsystem::VerifyCoreAcceptance() { return Finish(gr::runVerification(Session)); }
FGarageActionResult UGarageSessionSubsystem::CollectPayment() { return Finish(gr::completeOrder(Session)); }
void UGarageSessionSubsystem::RecordPlayerPosition(const FVector& P) {
    if (HasStarted && !P.ContainsNaN()) Session.playerPosition = {{P.X, P.Y, P.Z}};
}
