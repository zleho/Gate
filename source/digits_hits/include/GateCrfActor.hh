#pragma once

#include <memory>
#include <limits>
#include <string>

#include "GateVActor.hh"
#include "GateTreeFileManager.hh"

class GateCrfActorMessenger;

#define MAX_VOLUME_NAME 32

struct CrfEvent
{
    // incoming photon position on detector surface
    double in_x_mm;
    double in_y_mm;

    // incoming photon direction
    // spherical coordinates (physics convention) of unit direction
    double in_phi_rad; // azimuth
    double in_theta_rad; // polar

    // incoming energy of photon
    double in_energy_MeV;

    // energy is sum of all energy deposition (compton and photo) of photon
    // position is energy weighted position of all interactions within the crystal
    // no energy and spatial resolution is performed
    // if photon is not detected, then energy is zero
    double detected_x_mm;
    double detected_y_mm;
    double detected_energy_MeV;
    
    // before first interaction (with energy deposition) in crystal during current pass
    int compton_num;
    int rayleigh_num;
    
    bool primary_hit;
    bool secondary_hit;

    CrfEvent()
    {
        in_x_mm = in_y_mm = 0.0;
        in_phi_rad = in_theta_rad = 0.0;
        in_energy_MeV = 0.0;
        detected_x_mm = detected_y_mm = 0.0;
        detected_energy_MeV = 0.0;
        compton_num = 0;
        rayleigh_num = 0;
        primary_hit = false;
        secondary_hit = false;
    }
};

class GateCrfActor : public GateVActor
{
public:
    FCT_FOR_AUTO_CREATOR_ACTOR(GateCrfActor)
    virtual ~GateCrfActor();

    virtual void Construct() override;
    virtual void ResetData() override;
    virtual void SaveData() override;
    
    virtual void UserSteppingAction(const GateVVolume* volume, const G4Step* step) override;
    virtual void EndOfEventAction(const G4Event*) override;
    virtual void RecordEndOfAcquisition() override;

    void SetCrystalVolume(G4String volumeName);
    void SetOrientationX(G4ThreeVector v);
    void SetOrientationY(G4ThreeVector v);
    void Describe();
    void SetDebugLevel(int level)
    {
        _debugLevel = level;
    }

    void SetRrFactor(int factor)
    {
        if (factor >= 1)
        {
            _rrFactor = factor;
        }
    }
private:
    GateCrfActor(G4String name, G4int depth = 0);

    std::unique_ptr<GateCrfActorMessenger> _messenger;

    CrfEvent _eventToSave;
    bool _eventStarted;
    G4ThreeVector _orientation[3];
    GateVVolume* _crystalVolume;
    GateOutputTreeFileManager _file;
    int _debugLevel;
    int _rrFactor;
    int _rrCounter;
};

MAKE_AUTO_CREATOR_ACTOR(CrfActor, GateCrfActor)
