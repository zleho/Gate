#include <G4VEmProcess.hh>

#include "GateCrfActor.hh"
#include "GateCrfActorMessenger.hh"
#include "GateObjectStore.hh"
#include "GateMiscFunctions.hh"
#include "G4Gamma.hh"

std::ostream& operator<<(std::ostream& os, const CrfEvent& event)
{
    os << "(in_x_mm=" << event.in_x_mm;
    os << ",in_y_mm=" << event.in_y_mm;
    os << ",in_phi_rad=" << event.in_phi_rad;
    os << ",in_theta_mm=" << event.in_theta_rad;
    os << ",in_energy_MeV=" << event.in_energy_MeV;
    os << ",detected_x_mm=" << event.detected_x_mm;
    os << ",detected_y_mm=" << event.detected_y_mm;
    os << ",detected_energy_MeV=" << event.detected_energy_MeV;
    os << ",compton_num=" << event.compton_num;
    os << ",rayleigh_num=" << event.rayleigh_num;
    os << ",primary_hit=" << event.primary_hit;
    os << ",secondary_hit=" << event.secondary_hit << ")";
    return os;
}

GateCrfActor::GateCrfActor(G4String name, G4int depth)
    : GateVActor(name, depth), _messenger(new GateCrfActorMessenger(this)),
      _crystalVolume(nullptr), _debugLevel(0), _rrFactor(1)
{
    _orientation[0] = G4ThreeVector(1.0, 0.0, 0.0);
    _orientation[1] = G4ThreeVector(0.0, 1.0, 0.0);
    _orientation[2] = G4ThreeVector(0.0, 0.0, 1.0);
    ResetData();
}

GateCrfActor::~GateCrfActor()
{
}

void GateCrfActor::Construct()
{
    GateVActor::Construct();
    EnableBeginOfEventAction(false);
    EnableEndOfRunAction(false);
    EnableEndOfEventAction(true);
    EnableUserSteppingAction(true);
    EnableRecordEndOfAcquisition(true);

    if (!mVolume)
    {
        GateError("Crf actor is not attached to any volume");
    }

    if (mVolumeName == "world")
    {
        GateError("Crf actor is attach to world. You must attach it to a SPECT head.");
    }

    if (!_crystalVolume)
    {
        GateError("there is not crystal defined for Crf actor");
    }

    auto vol = _crystalVolume;
    bool ancenstorFound = false;
    while (vol->GetObjectName() != "world")
    {
        if (vol->GetVolumeNumber() > 1)
        {
            GateError("CrfActor: there is more than one crystal volumes defined in geometry.");
        }

        vol = vol->GetMotherCreator();

        if (vol == mVolume)
        {
            ancenstorFound = true;
        }
    }

    if (!ancenstorFound)
    {
        GateError(_crystalVolume->GetObjectName() + " is not descendent of actor's attached volume: " + mVolumeName);
    }

    _orientation[2] = _orientation[0].cross(_orientation[1]);

    if (mSaveFilename == "")
    {
        GateError("no savfile set in Crf actor");
    }

    _file.add_file(mSaveFilename, getExtension(mSaveFilename));
    _file.set_tree_name("CrfEventStats");
    _file.write_variable("in_x_mm", &_eventToSave.in_x_mm);
    _file.write_variable("in_y_mm", &_eventToSave.in_y_mm);
    _file.write_variable("in_phi_rad", &_eventToSave.in_phi_rad);
    _file.write_variable("in_theta_rad", &_eventToSave.in_theta_rad);
    _file.write_variable("in_energy_MeV", &_eventToSave.in_energy_MeV);
    _file.write_variable("detected_x_mm", &_eventToSave.detected_x_mm);
    _file.write_variable("detected_y_mm", &_eventToSave.detected_y_mm);
    _file.write_variable("detected_energy_MeV", &_eventToSave.detected_energy_MeV);
    _file.write_variable("compton_num", &_eventToSave.compton_num);
    _file.write_variable("rayleigh_num", &_eventToSave.rayleigh_num);
    _file.write_variable("primary_hit", &_eventToSave.primary_hit);
    _file.write_variable("secondary_hit", &_eventToSave.secondary_hit);
    _file.write_header();
}

void GateCrfActor::ResetData()
{
    _eventStarted = false;
    _rrCounter = 1;
}

void GateCrfActor::SaveData()
{
}

void GateCrfActor::SetCrystalVolume(G4String volumeName)
{
    _crystalVolume = GateObjectStore::GetInstance()->FindVolumeCreator(volumeName);
    if (!_crystalVolume)
    {
        GateWarning("CrfActor: cannot find crystal volume with name " + volumeName);
    }
}

void GateCrfActor::SetOrientationX(G4ThreeVector v)
{
    _orientation[0] = v.unit();
    _orientation[2] = _orientation[0].cross(_orientation[1]);
}

void GateCrfActor::SetOrientationY(G4ThreeVector v)
{
    _orientation[1] = v.unit();
    _orientation[2] = _orientation[0].cross(_orientation[1]);
}

void GateCrfActor::Describe()
{
    G4cout << "Crf actor: " << GetObjectName() << "\n"
           << "Attached volume: " << mVolumeName << "\n"
           << "Crystal volume:" << (_crystalVolume ? _crystalVolume->GetObjectName() : G4String("NULL")) << "\n"
           << "Orientation:\n"
           << _orientation[0][0] << ' ' << _orientation[1][0] << ' ' << _orientation[2][0] << "\n"
           << _orientation[0][1] << ' ' << _orientation[1][1] << ' ' << _orientation[2][1] << "\n"
           << _orientation[0][2] << ' ' << _orientation[1][2] << ' ' << _orientation[2][2] << "\n";
}

void GateCrfActor::UserSteppingAction(const GateVVolume*, const G4Step* step)
{
    if (step->GetTrack()->GetParticleDefinition() != G4Gamma::Gamma())
    {
        return;
    }

    if (!_eventStarted)
    {
        _eventStarted = true;
        _eventToSave = CrfEvent();

        // this is the first step in SPECT head
        // we assume that this is the primary photon

        auto start_pos = step->GetTrack()->GetVertexPosition();
        auto start_dir = step->GetTrack()->GetVertexMomentumDirection();

        if (_debugLevel > 1)
        {
            G4cout << "start_dir=(" << start_dir.phi() << "," << start_dir.theta() << ")\n";
        }

        // transform pos and direction into camera orientation
        // X is along width
        // Y is along height
        // Z=cross(X, Y) is into the camera
        // here we assume that (0,0) is the detector center on the detector's plane
        _eventToSave.in_x_mm = start_pos.dot(_orientation[0]);
        _eventToSave.in_y_mm = start_pos.dot(_orientation[1]);

        G4ThreeVector start_local_dir(
            start_dir.dot(_orientation[0]),
            start_dir.dot(_orientation[1]),
            start_dir.dot(_orientation[2])
        );

        if (_debugLevel > 1)
        {
            G4cout << "start_local_dir=(" << start_local_dir.phi() << "," << start_local_dir.theta() << ")\n";
        }

        _eventToSave.in_phi_rad = start_local_dir.phi();
        _eventToSave.in_theta_rad = start_local_dir.theta();

        _eventToSave.in_energy_MeV = step->GetTrack()->GetVertexKineticEnergy();
    }

    const G4VEmProcess *process = dynamic_cast<const G4VEmProcess*>(
        step->GetPostStepPoint()->GetProcessDefinedStep()
    );

    if(!process)
    {
        // we don't care about non-EM processes
        return;
    }

    if (step->GetPostStepPoint()->GetPhysicalVolume() == _crystalVolume->GetPhysicalVolume())
    {
        if (process->GetProcessName() == "Compton" || process->GetProcessName() == "compt" ||
            process->GetProcessName() == "PhotoElectric" || process->GetProcessName() == "phot")
        {
            // G4cout << "compton or photoelectric in crystal\n";
            double energy_MeV = step->GetTotalEnergyDeposit();
            const auto* theTouchable = dynamic_cast<const G4TouchableHistory*>(step->GetPostStepPoint()->GetTouchable());

            // transform position into world frame
            auto local_pos = theTouchable->GetHistory()->GetTopTransform().TransformPoint(step->GetPostStepPoint()->GetPosition());
            double x_mm = local_pos.dot(_orientation[0]);
            double y_mm = local_pos.dot(_orientation[1]);

            // running weighted average with energy of positions
            if (_debugLevel > 1)
            {
                G4cout << local_pos << '\n';
                G4cout << "before: " << _eventToSave << '\n';
            }
            
            _eventToSave.detected_x_mm = _eventToSave.detected_x_mm * _eventToSave.detected_energy_MeV + x_mm * energy_MeV;
            _eventToSave.detected_y_mm = _eventToSave.detected_y_mm * _eventToSave.detected_energy_MeV + y_mm * energy_MeV;
            _eventToSave.detected_energy_MeV += energy_MeV;
            _eventToSave.detected_x_mm /= _eventToSave.detected_energy_MeV;
            _eventToSave.detected_y_mm /= _eventToSave.detected_energy_MeV;

            if (step->GetTrack()->GetTrackID() == 1)
            {
                _eventToSave.primary_hit = true;
            }
            else
            {
                _eventToSave.secondary_hit = true;
            }

            if (_debugLevel > 1)
            {
                G4cout << "after: " << _eventToSave << '\n';
            }
        }
    }
    else
    {
        if (process->GetProcessName() == "Compton" || process->GetProcessName() == "compt")
        {
            _eventToSave.compton_num += 1;
        }
        else if (process->GetProcessName() == "RayleighScattering" || process->GetProcessName() == "Rayl")
        {
            _eventToSave.rayleigh_num += 1;
        }
    }
}

void GateCrfActor::EndOfEventAction(const G4Event* ev)
{
    if (_eventStarted)
    {
        bool saveEvent = false;
        if (_eventToSave.detected_energy_MeV > 0)
        {
            saveEvent = true;
        }
        else
        {
            if (_rrCounter == _rrFactor)
            {
                saveEvent = true;
                _rrCounter = 1;
            }
            else
            {
                if (_debugLevel > 0)
                {
                    G4cout << "skipped event:" << _eventToSave << "\n";
                }
                ++_rrCounter;
            }
        }

        if (saveEvent)
        {
            _file.fill();
            if (_debugLevel > 0)
            {
                G4cout << _eventToSave << '\n';
            }
        }

        _eventStarted = false;
    }
}

void GateCrfActor::RecordEndOfAcquisition()
{
    _file.close();
}