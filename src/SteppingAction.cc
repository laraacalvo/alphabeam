//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
//
#include "SteppingAction.hh"
#include "G4AnalysisManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"
#include <map>
#include "globals.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4RunManager.hh"
#include "DetectorConstruction.hh"
#include "CommandLineParser.hh"
#include "EventAction.hh"
#include "G4Ions.hh"
#include <cmath>

using namespace G4DNAPARSER;
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

SteppingAction::SteppingAction(DetectorConstruction *pDetector)
    : G4UserSteppingAction(), fpEventAction(0), fDetector(pDetector)
{
  fpEventAction = (EventAction *)G4EventManager::GetEventManager()->GetUserEventAction();
  fRunAction = (RunAction *)(G4RunManager::GetRunManager()->GetUserRunAction());

  CommandLineParser *parser = CommandLineParser::GetParser();
  Command *command(0);
  G4String fileName{"PSfile.bin"};
  if ((command = parser->GetCommandIfActive("-out")) != 0)

    if (command->GetOption().empty() == false)
    {
      fileName = command->GetOption() + ".bin";
    }

  PSfile.open(fileName, std::ios::out | std::ios::binary);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

SteppingAction::~SteppingAction()
{
  PSfile.close();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
void SteppingAction::UserSteppingAction(const G4Step *step)
{
  // G4cout << "start of step, volume = " << step->GetPreStepPoint()->GetPhysicalVolume()->GetName()<< " step length = " << step->GetStepLength()/nm << G4endl;

  if (step->GetTrack()->GetParticleDefinition()->GetParticleName() == "anti_nu_e") // not anti neutrinos
    return;

  G4String particleName = step->GetTrack()->GetParticleDefinition()->GetParticleName();

  // if (step->GetTrack()->GetCreatorProcess() != nullptr)
  // {
    // G4cout << particleName << " Track ID = " << step->GetTrack()->GetTrackID() << " creator process = " << step->GetTrack()->GetCreatorProcess()->GetProcessName() << " KE = " << step->GetPreStepPoint()->GetKineticEnergy() << " parent = " << step->GetTrack()->GetParentID() << "position = " << step->GetPreStepPoint()->GetPosition() << ", process = " << step->GetPostStepPoint()->GetProcessDefinedStep()->GetProcessName() << " " << fpEventAction->parentParticle[step->GetTrack()->GetParentID()] << G4endl;
    // G4cout << particleName << " " << step->GetPreStepPoint()->GetKineticEnergy() << " " << step->GetPostStepPoint()->GetPhysicalVolume()->GetName() << " " << step->GetPostStepPoint()->GetPosition()<< G4endl;
  // }

  G4int TrackID = step->GetTrack()->GetTrackID();
  if ((fpEventAction->parentParticle.find(TrackID) == fpEventAction->parentParticle.end()) && (step->GetTrack()->GetCreatorProcess() != nullptr))
  {
    // track ID not found, save which to map, trackID: creator particle from decay (e-,alpha, gamma) for split of DNA damage by source
    if (step->GetTrack()->GetCreatorProcess()->GetProcessName() == "RadioactiveDecay")
    {
      if (G4StrUtil::contains(particleName, "[")) // to catch excited states
      {
        fpEventAction->parentParticle.insert(std::pair<G4int, G4int>(TrackID, particleOriginMap[particleName.substr(0, 5)]));
      }
      if ((particleName == "e-") || (particleName == "gamma") || (particleName == "alpha") || (particleName == "e+")) // save product and parent names
      {
        G4String parentName = reverseParticleOriginMap[fpEventAction->parentParticle[step->GetTrack()->GetParentID()]];
        G4String combinedName = particleName + parentName;

        fpEventAction->parentParticle.insert(std::pair<G4int, G4int>(TrackID, particleOriginMap[combinedName]));
      }
      else
      {
        fpEventAction->parentParticle.insert(std::pair<G4int, G4int>(TrackID, particleOriginMap[particleName]));
      }
    }
    else
    {
      // not radioactive decay so another process so parent ID should be in mapping
      G4int parentParticle = fpEventAction->parentParticle[step->GetTrack()->GetParentID()];
      // add current track with parent particle
      fpEventAction->parentParticle.insert(std::pair<G4int, G4int>(TrackID, parentParticle));
    }
  }

  if (step->GetPostStepPoint()->GetPhysicalVolume()->GetName() == "world")
  {
    step->GetTrack()->SetTrackStatus(fKillTrackAndSecondaries);

    return;
  }

  if ((particleName == "Rn220") && (step->GetPreStepPoint()->GetKineticEnergy() == 0))
  {
    // Desorption from the source through recoil only
    if (step->GetPreStepPoint()->GetPhysicalVolume()->GetName() == "seed")
    {
      fpEventAction->addRnDesorptionIN();
    }
  }
  if ((G4StrUtil::contains(particleName, "Pb212")) && (step->GetPreStepPoint()->GetKineticEnergy() == 0))
  {
    if (step->GetPreStepPoint()->GetPhysicalVolume()->GetName() == "seed")
    {
      fpEventAction->addPbDesorptionIN();
    }
  }

  G4double particleEnergy = step->GetPreStepPoint()->GetKineticEnergy();
  if ((particleName == "Pb212") && (particleEnergy == 0))
  {
    if (step->GetPreStepPoint()->GetPhysicalVolume()->GetName() != "seed")
    {
      G4double alpha = 0.6931471806 / (10.64 * 60 * 60); // clearance rate for Pb due to vascular routes, equal to lambda pb resulting in a leakage of 50% Phys. Med. Biol. 65 (2020)
      G4double leakTime = step->GetPostStepPoint()->GetLocalTime() / s;
      G4double p = alpha * leakTime;
      p = std::pow(2.718, -1 * p);

      if (p <= G4UniformRand())
      {
        step->GetTrack()->SetTrackStatus(fKillTrackAndSecondaries);
        fpEventAction->addPbLeakage();
        return;
      }
      else
      {
        fpEventAction->addPbNoLeakage();
      }
    }
  }
  if ((particleName == "Pb208") && (particleEnergy == 0))
  {
    step->GetTrack()->SetTrackStatus(fKillTrackAndSecondaries);
    return;
  }
  CommandLineParser *parser = CommandLineParser::GetParser();
  Command *command(0);

  if ((command = parser->GetCommandIfActive("-out")) == 0)
    return;

  if (step->GetPreStepPoint() == nullptr)
    return;

  G4String volumeNamePre = step->GetPreStepPoint()->GetPhysicalVolume()->GetName();
  G4AnalysisManager *analysisManager = G4AnalysisManager::Instance();

  // Calculate dose for all rings
  if (volumeNamePre == "cell")
  {
    G4double radius = fDetector->R[step->GetPreStepPoint()->GetPhysicalVolume()->GetCopyNo()] / um;
    G4double edep = step->GetTotalEnergyDeposit() / joule;

    G4double volumeCube = 300e-9 * 300e-9 * 300e-9;
    G4double density = 1000; // water
    G4double massCube = density * volumeCube;

    analysisManager->FillH1(0, radius, edep/massCube);
  }

  // save all steps entering cube
  if ((volumeNamePre == "water") && (step->GetPostStepPoint()->GetPhysicalVolume()->GetName() == "cell"))
  {
    if (step->GetPreStepPoint()->GetKineticEnergy() > 0)
    {
      G4TouchableHandle theTouchable = step->GetPostStepPoint()->GetTouchableHandle();
      G4ThreeVector worldPos = step->GetPostStepPoint()->GetPosition();
      G4ThreeVector localPos = theTouchable->GetHistory()->GetTopTransform().TransformPoint(worldPos);

      G4ThreeVector worldMomentum = step->GetPostStepPoint()->GetMomentumDirection();
      G4ThreeVector localMomentum = (*(theTouchable->GetHistory()->GetTopVolume()->GetRotation()))*worldMomentum; // rotate momentum direction by volume rotation

      G4double time = step->GetPostStepPoint()->GetGlobalTime();

      auto particleEnergy = step->GetPostStepPoint()->GetKineticEnergy();
      G4int eventID = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();

      G4String particleName = step->GetTrack()->GetParticleDefinition()->GetParticleName();

      savePoint(step->GetTrack(),  localPos, localMomentum, step->GetPostStepPoint()->GetPhysicalVolume()->GetCopyNo(),  particleEnergy,  time,  0);
    }
  }
  // save decay in box
  else if ((volumeNamePre == "cell") && (step->IsFirstStepInVolume())) // save particles created in the cell or nucleus if from radioactive decay as not simulated in RBE
  {
    
    if (step->GetPreStepPoint()->GetProcessDefinedStep() == nullptr)
    // if prestep process is nullptr this is the first step of particle created by interaction in the cell - only save those created by processes in cell not in other volumes
    {
      if ((step->GetTrack()->GetCreatorProcess()->GetProcessName() == "RadioactiveDecay") && (((const G4Ions *)(step->GetTrack()->GetParticleDefinition()))->GetExcitationEnergy() < 1e-10))
   {
    G4TouchableHandle theTouchable = step->GetPreStepPoint()->GetTouchableHandle();
    G4ThreeVector worldPos = step->GetPreStepPoint()->GetPosition();
    G4ThreeVector localPos = theTouchable->GetHistory()->GetTopTransform().TransformPoint(worldPos);

    G4ThreeVector worldMomentum = step->GetPreStepPoint()->GetMomentumDirection();
    G4ThreeVector localMomentum = (*(theTouchable->GetHistory()->GetTopVolume()->GetRotation()))*worldMomentum;

    G4double time = step->GetPreStepPoint()->GetGlobalTime();

    auto particleEnergy = step->GetPreStepPoint()->GetKineticEnergy();
    G4int eventID = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();

    G4String particleName = step->GetTrack()->GetParticleDefinition()->GetParticleName();
      savePoint(step->GetTrack(),  localPos, localMomentum, step->GetPostStepPoint()->GetPhysicalVolume()->GetCopyNo(),  particleEnergy,  time,  0);

   }
    }
  }

}

void SteppingAction::savePoint(const G4Track *track, G4ThreeVector newPos, G4ThreeVector boxMomentum, const int copy, G4double particleEnergy, G4double time, G4int originParticle)
{
  // save particle to phase space file in box reference frame

  fpEventAction->particlePos.erase(track->GetTrackID()); // erase current saved box entry position for this track

  fpEventAction->particlePos.insert(std::pair<int, G4ThreeVector>(track->GetTrackID(), newPos)); // add current box entry position for this track

  fpEventAction->particleDist.erase(track->GetTrackID());                                                  // erase distance travelled for this track in the box
  fpEventAction->particleDist.insert(std::pair<int, G4ThreeVector>(track->GetTrackID(), G4ThreeVector())); // made initial distance travelled zero

  G4int eventID = G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID();

  G4String particleName = track->GetParticleDefinition()->GetParticleName();

  G4float particleID = particleMap[particleName];
  if (particleID == 0)
  {
    G4cout << particleName << "  not saved" << G4endl;
    return;
  }
  double output[12];
  output[0] = newPos.x() / mm;
  output[1] = newPos.y() / mm;
  output[2] = newPos.z() / mm;
  output[3] = boxMomentum.x();
  output[4] = boxMomentum.y();
  output[5] = boxMomentum.z();
  output[6] = particleEnergy / MeV;
  output[7] = eventID;
  output[8] = particleID;
  output[9] = copy;
  output[10] = time / s;
  output[11] = originParticle;

  PSfile.write((char *)&output, sizeof(output));
  fpEventAction->tracks.push_back(track->GetTrackID());

  // G4cout << particleName << " saved at = " << newPos / mm << " with KE = " << particleEnergy << " with momentum " << boxMomentum << " TracKID = " << track->GetTrackID() << " originParticle "<< originParticle << G4endl;
}

G4ThreeVector SteppingAction::transformDirection(G4ThreeVector position, G4ThreeVector worldMomentum)
{
  G4double theta = std::asin(position.x() / std::pow(position.y() * position.y() + position.x() * position.x(), 0.5));
  if ((position.x() > 0) && (position.y() > 0))
    // # positive-positive quadrant
    theta = theta;
  else if ((position.x() > 0) && (position.y() < 0))
    // # positive-negative quadrant
    theta = 3.14159 - theta;
  else if ((position.x() < 0) && (position.y() < 0))
    // # negative-negative quadrant
    theta = std::abs(theta) + 3.14159;
  else if ((position.x() < 0) && (position.y() > 0))
    // # negative-positive quadrant
    theta = theta;

  G4ThreeVector newMomentum = G4ThreeVector(worldMomentum.x() * std::cos(theta) - worldMomentum.y() * std::sin(theta), worldMomentum.x() * std::sin(theta) + worldMomentum.y() * std::cos(theta), worldMomentum.z());

  return newMomentum;
}

G4double SteppingAction::calculateDistanceToExitBox(G4ThreeVector preStepPosition, G4ThreeVector preStepMomentumDirection)
{
  // does step exit box in x and z?
  G4double tXneg = (-.00015 - preStepPosition.x()) / preStepMomentumDirection.x();
  G4double tXpos = (.00015 - preStepPosition.x()) / preStepMomentumDirection.x();

  G4double tYneg = (-.00015 - preStepPosition.y()) / preStepMomentumDirection.y();
  G4double tYpos = (.00015 - preStepPosition.y()) / preStepMomentumDirection.y();

  G4double tZneg = (-.00015 - preStepPosition.z()) / preStepMomentumDirection.z();
  G4double tZpos = (.00015 - preStepPosition.z()) / preStepMomentumDirection.z();

  tXneg = tXneg > 0 ? tXneg : DBL_MAX;
  tXpos = tXpos > 0 ? tXpos : DBL_MAX;
  tYneg = tYneg > 0 ? tYneg : DBL_MAX;
  tYpos = tYpos > 0 ? tYpos : DBL_MAX;
  tZneg = tZneg > 0 ? tZneg : DBL_MAX;
  tZpos = tZpos > 0 ? tZpos : DBL_MAX;

  G4double distanceToExit = std::min({tXneg, tXpos, tZneg, tZpos}); // shortest distance travelled to cross box surface

  // G4cout << tXneg << " " << tXpos << " " << tYneg << " " << tYpos << " " << tZneg << " " << tZpos << " " << G4endl;
  if ((tYneg <= distanceToExit) || (tYpos <= distanceToExit)) // exit y
    return DBL_MAX;

  return distanceToExit;
}