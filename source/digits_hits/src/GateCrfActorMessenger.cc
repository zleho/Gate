#include "GateCrfActorMessenger.hh"
#include "GateCrfActor.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWith3Vector.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcmdWithAnInteger.hh"

GateCrfActorMessenger::GateCrfActorMessenger(GateCrfActor* sensor)
    : GateActorMessenger(sensor), _sensor(sensor)
{
    BuildCommands(baseName + _sensor->GetObjectName());
}

GateCrfActorMessenger::~GateCrfActorMessenger()
{
}

void GateCrfActorMessenger::BuildCommands(G4String base)
{
    _setCrystalCmd = std::make_unique<G4UIcmdWithAString>(G4String(base + "/setCrystal"), this);
    _setCrystalCmd->SetGuidance("Set crystal volume name for Crf Actor. The volume or any asncestor cannot be a repeated volume.");

    _setOrientationXCmd = std::make_unique<G4UIcmdWith3Vector>(G4String(base + "/setOrientationX"), this);
    _setOrientationXCmd->SetGuidance("Set the first orientation vector of the detector's system. Default orientation is 1 0 0.");

    _setOrientationYCmd = std::make_unique<G4UIcmdWith3Vector>(G4String(base + "/setOrientationY"), this);
    _setOrientationYCmd->SetGuidance("Set the second orientation vector of the detector's system. Default orientation is 0 1 0.");

    _describeCmd = std::make_unique<G4UIcmdWithoutParameter>(G4String(base + "/describe"), this);
    _describeCmd->SetGuidance("Describe the Crf Actor.");

    _setDebugLevelCmd = std::make_unique<G4UIcmdWithAnInteger>(G4String(base + "/setDebugLevel"), this);
    _setDebugLevelCmd->SetGuidance("Set Debug level.");
}

void GateCrfActorMessenger::SetNewValue(G4UIcommand *cmd, G4String newValue)
{
    GateActorMessenger::SetNewValue(cmd, newValue);
    
    if (cmd == _setCrystalCmd.get())
    {
        _sensor->SetCrystalVolume(newValue);
    }
    else if (cmd == _setOrientationXCmd.get())
    {
        _sensor->SetOrientationX(_setOrientationXCmd->ConvertTo3Vector(newValue));
    }
    else if (cmd == _setOrientationYCmd.get())
    {
        _sensor->SetOrientationY(_setOrientationYCmd->ConvertTo3Vector(newValue));

    }
    else if (cmd == _describeCmd.get())
    {
        _sensor->Describe();
    }
    else if (cmd == _setDebugLevelCmd.get())
    {
        _sensor->SetDebugLevel(_setDebugLevelCmd->ConvertToInt(newValue));
    }
}