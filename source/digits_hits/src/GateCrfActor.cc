#include <G4VEmProcess.hh>

#include "GateCrfActor.hh"
#include "GateCrfActorMessenger.hh"
#include "GateObjectStore.hh"
#include "GateMiscFunctions.hh"

GateCrfActor::GateCrfActor(G4String name, G4int depth)
    : GateVActor(name, depth), _messenger(new GateCrfActorMessenger(this)),
      _crystalVolume(nullptr), _debugLevel(0)
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
    _file.write_variable("event_id", &_eventToSave.event_id);
    _file.write_variable("in_x_mm", &_eventToSave.in_x_mm);
    _file.write_variable("in_y_mm", &_eventToSave.in_y_mm);
    _file.write_variable("in_phi_rad", &_eventToSave.in_phi_rad);
    _file.write_variable("in_theta_rad", &_eventToSave.in_theta_rad);
    _file.write_variable("in_energy_MeV", &_eventToSave.in_energy_MeV);
    _file.write_variable("photon_id", &_eventToSave.photon_id);
    _file.write_variable("crystal_pass", &_eventToSave.crystal_pass);
    _file.write_variable("detected_x_mm", &_eventToSave.detected_x_mm);
    _file.write_variable("detected_y_mm", &_eventToSave.detected_y_mm);
    _file.write_variable("detected_energy_MeV", &_eventToSave.detected_energy_MeV);
    _file.write_variable("compton_num", &_eventToSave.compton_num);
    _file.write_variable("rayleigh_num", &_eventToSave.rayleigh_num);
    _file.write_variable("compton_volume", _eventToSave.compton_volume, MAX_VOLUME_NAME);
    _file.write_variable("rayleigh_volume", _eventToSave.rayleigh_volume, MAX_VOLUME_NAME);
    _file.write_header();
}

void GateCrfActor::ResetData()
{
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
    const G4VEmProcess *process = dynamic_cast<const G4VEmProcess*>(
        step->GetPostStepPoint()->GetProcessDefinedStep()
    );
    
    if(!process)
    {
        // we don't care about non-EM processes
        return;
    }

    if (_events.empty())
    {
        // this is the first EM step in SPECT head
        // we assume that this is the primary photon
        _events.emplace_back();

        // transform position and direction into current volume's coordinate system
        const auto* theTouchable = dynamic_cast<const G4TouchableHistory*>(step->GetPreStepPoint()->GetTouchable());
        auto start_pos = theTouchable->GetHistory()->GetTopTransform().TransformPoint(step->GetTrack()->GetVertexPosition());
        auto start_dir = theTouchable->GetHistory()->GetTopTransform().TransformAxis(step->GetTrack()->GetVertexMomentumDirection());

        if (_debugLevel > 1)
        {
            G4cout << "start_dir=(" << start_dir.phi() << "," << start_dir.theta() << ")\n";
        }

        // transform pos and direction into camera orientation
        // X is along width
        // Y is along height
        // Z=cross(X, Y) is into the camera
        _events.back().in_x_mm = start_pos.dot(_orientation[0]);
        _events.back().in_y_mm = start_pos.dot(_orientation[1]);

        G4ThreeVector start_local_dir(
            start_dir.dot(_orientation[0]),
            start_dir.dot(_orientation[1]),
            start_dir.dot(_orientation[2])
        );

        if (_debugLevel > 1)
        {
            G4cout << "start_local_dir=(" << start_local_dir.phi() << "," << start_local_dir.theta() << ")\n";
        }

        _events.back().in_phi_rad = start_local_dir.phi();
        _events.back().in_theta_rad = start_local_dir.theta();

        _events.back().in_energy_MeV = step->GetTrack()->GetVertexKineticEnergy();
        _events.back().photon_id = step->GetTrack()->GetTrackID();
    }

    if (step->GetTrack()->GetTrackID() != _events.back().photon_id)
    {
        // this is the first EM step of a secondary photon;
        _events.emplace_back();
        const auto& last_event = _events[_events.size() - 2];
        _events.back().in_x_mm = last_event.in_x_mm;
        _events.back().in_y_mm = last_event.in_y_mm;
        _events.back().in_phi_rad = last_event.in_phi_rad;
        _events.back().in_theta_rad = last_event.in_theta_rad;
        _events.back().in_energy_MeV = last_event.in_energy_MeV;
        _events.back().photon_id = step->GetTrack()->GetTrackID();
    }

    if (step->GetPostStepPoint()->GetPhysicalVolume() == _crystalVolume->GetPhysicalVolume())
    {
        if (process->GetProcessName() == "Compton" || process->GetProcessName() == "compt" ||
            process->GetProcessName() == "PhotoElectric" || process->GetProcessName() == "phot")
        {
            // G4cout << "compton or photoelevtric in crystal\n";
            double energy_MeV = step->GetTotalEnergyDeposit();
            const auto* theTouchable = dynamic_cast<const G4TouchableHistory*>(step->GetPreStepPoint()->GetTouchable());
            auto local_pos = theTouchable->GetHistory()->GetTopTransform().TransformPoint(step->GetPreStepPoint()->GetPosition());
            double x_mm = local_pos.dot(_orientation[0]);
            double y_mm = local_pos.dot(_orientation[1]);

            auto& current_event = _events.back();

            // running weighted average with energy of positions
            current_event.detected_x_mm = current_event.detected_x_mm * current_event.detected_energy_MeV + x_mm * energy_MeV;
            current_event.detected_y_mm = current_event.detected_y_mm * current_event.detected_energy_MeV + y_mm * energy_MeV;
            current_event.detected_energy_MeV += energy_MeV;
            current_event.detected_x_mm /= current_event.detected_energy_MeV;
            current_event.detected_y_mm /= current_event.detected_energy_MeV;
        }
    }
    else
    {
        // G4cout << "EM step in non-crystal\n";

        if (_events.back().detected_energy_MeV > 0.0)
        {
            // we have stepped out from the crystal after energy deposition in crystal;
            _events.emplace_back();
            const auto& last_event = _events[_events.size() - 2];
            _events.back().in_x_mm = last_event.in_x_mm;
            _events.back().in_y_mm = last_event.in_y_mm;
            _events.back().in_phi_rad = last_event.in_phi_rad;
            _events.back().in_theta_rad = last_event.in_theta_rad;
            _events.back().in_energy_MeV = last_event.in_energy_MeV;
            _events.back().photon_id = last_event.photon_id;
            _events.back().crystal_pass = last_event.crystal_pass + 1;
        }

        if (process->GetProcessName() == "Compton" || process->GetProcessName() == "compt")
        {
            strcpy(_events.back().compton_volume, step->GetPostStepPoint()->GetPhysicalVolume()->GetName().c_str());
            _events.back().compton_num += 1;
        }
        else if (process->GetProcessName() == "RayleighScattering" || process->GetProcessName() == "Rayl")
        {
            strcpy(_events.back().rayleigh_volume, step->GetPostStepPoint()->GetPhysicalVolume()->GetName().c_str());
            _events.back().rayleigh_num += 1;
        }
    }
}

std::ostream& operator<<(std::ostream& os, const CrfEventStats& event)
{
    os << "(event_id=" << event.event_id;
    os << ",in_x_mm=" << event.in_x_mm;
    os << ",in_y_mm=" << event.in_y_mm;
    os << ",in_phi_rad=" << event.in_phi_rad;
    os << ",in_theta_mm=" << event.in_theta_rad;
    os << ",in_energy_MeV=" << event.in_energy_MeV;
    os << ",photon_id=" << event.photon_id;
    os << ",crystal_pass=" << event.crystal_pass;
    os << ",detected_x_mm=" << event.detected_x_mm;
    os << ",detected_y_mm=" << event.detected_y_mm;
    os << ",detected_energy_MeV=" << event.detected_energy_MeV;
    os << ",compton_num=" << event.compton_num;
    os << ",rayleigh_num=" << event.rayleigh_num;
    os << ",compton_volume=" << event.compton_volume;
    os << ",rayleigh_volume=" << event.rayleigh_volume << ")";
    return os;
}

void GateCrfActor::EndOfEventAction(const G4Event* ev)
{
    for (const auto& event : _events)
    {
        _eventToSave = event;
        _eventToSave.event_id = ev->GetEventID();
        if (_debugLevel > 0)
        {
            G4cout << _eventToSave << '\n';
        }
        _file.fill();
    }

    _events.clear();
}

void GateCrfActor::RecordEndOfAcquisition()
{
    _file.close();
}