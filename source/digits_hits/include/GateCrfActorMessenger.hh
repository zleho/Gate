#pragma once

#include <memory>
#include "GateActorMessenger.hh"

class GateCrfActor;
class G4UIcmdWithAString;
class G4UIcmdWith3Vector;
class G4UIcmdWithoutParameter;
class G4UIcmdWithAnInteger;

class GateCrfActorMessenger : public GateActorMessenger
{
public:
    GateCrfActorMessenger(GateCrfActor* sensor);
    virtual ~GateCrfActorMessenger();
    void BuildCommands(G4String base);
    void SetNewValue(G4UIcommand *cmd, G4String newValue);
private:
    GateCrfActor* _sensor;
    std::unique_ptr<G4UIcmdWithAString> _setCrystalCmd;
    std::unique_ptr<G4UIcmdWith3Vector> _setOrientationXCmd;
    std::unique_ptr<G4UIcmdWith3Vector> _setOrientationYCmd;
    std::unique_ptr<G4UIcmdWithoutParameter> _describeCmd;
    std::unique_ptr<G4UIcmdWithAnInteger> _setDebugLevelCmd;
    std::unique_ptr<G4UIcmdWithAnInteger> _setRrFactorCmd;
};
