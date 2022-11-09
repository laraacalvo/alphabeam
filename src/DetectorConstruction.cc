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
/// \file DetectorConstruction.cc
/// \brief Implementation of the DetectorConstruction class

#include "DetectorConstruction.hh"

#include "globals.hh"
#include "G4LogicalVolume.hh"
#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4Orb.hh"
#include "G4VisAttributes.hh"
#include "G4NistManager.hh"
#include <G4SystemOfUnits.hh>
#include "G4UserLimits.hh"
#include "G4UnitsTable.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PVPlacement.hh"

#include "CommandLineParser.hh"

#include "G4Region.hh"
#include "G4RegionStore.hh"

#include "G4ProductionCuts.hh"
#include "G4RunManager.hh"
#include "DetectorMessenger.hh"

#include "RunAction.hh"

using namespace G4DNAPARSER;

#define countof(x) (sizeof(x) / sizeof(x[0]))

using namespace std;
using CLHEP::angstrom;
using CLHEP::degree;
using CLHEP::micrometer;
using CLHEP::mm;
using CLHEP::nanometer;

static G4VisAttributes visGrey(true, G4Colour(0.839216, 0.839216, 0.839216));
static G4VisAttributes invisGrey(false, G4Colour(0.839216, 0.839216, 0.839216));

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::DetectorConstruction() : G4VUserDetectorConstruction()
{
  R = {155 * micrometer, 175 * micrometer, 195 * micrometer, 215 * micrometer, 235 * micrometer, 255 * micrometer, 275 * micrometer, 295 * micrometer, 315 * micrometer, 335 * micrometer};

  fDetectorMessenger = new DetectorMessenger(this);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::~DetectorConstruction()
{
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume *DetectorConstruction::Construct()
{

  /***************************************************************************/
  //                               World
  /***************************************************************************/

  G4NistManager *man = G4NistManager::Instance();
  G4Material *waterMaterial = man->FindOrBuildMaterial("G4_WATER");
  G4Material *S_Steel = G4NistManager::Instance()->FindOrBuildMaterial("G4_STAINLESS-STEEL");

  G4Box *solidWorld = new G4Box("world", 1000 * mm, 1000 * mm, 1000 * mm);

  G4Tubs *solidSeed = new G4Tubs("seed", 0., 0.15 * mm, 3 * mm, 0, 360 * degree);
  G4Orb *solidCell = new G4Orb("cell", 5 * micrometer);
  G4Orb *solidNucleus = new G4Orb("nucleus", 2.5 * micrometer);

  G4LogicalVolume *logicWorld = new G4LogicalVolume(solidWorld,
                                                    waterMaterial,
                                                    "world");

  G4PVPlacement *physiWorld = new G4PVPlacement(0,
                                                G4ThreeVector(),
                                                "world",
                                                logicWorld,
                                                0,
                                                false,
                                                0);

  G4LogicalVolume *logicSeed = new G4LogicalVolume(solidSeed,
                                                   S_Steel,
                                                   "seed");

  G4PVPlacement *physiSeed = new G4PVPlacement(0,
                                               G4ThreeVector(),
                                               logicSeed,
                                               "seed",
                                               logicWorld,
                                               0,
                                               false,
                                               0);

  SetCells(Rmin, Rmax);

  for (G4int r = 0; r < R.size(); ++r)
  {
    G4int numCells{0};
    for (G4int z = -100; z <= 100; z += 10)
    {
      for (float theta = 0; theta <= 2 * 3.14159 - 10.5 * micrometer / R[r]; theta += 10.5 * micrometer / R[r])
      {
        // G4LogicalVolume *logicCell = new G4LogicalVolume(solidCell,
        //                                                  waterMaterial,
        //                                                  "cell");

        // G4PVPlacement *physiCell = new G4PVPlacement(0,
        //                                              G4ThreeVector(R[r] * cos(theta), R[r] * sin(theta), z * micrometer),
        //                                              logicCell,
        //                                              "cell",
        //                                              logicWorld,
        //                                              0,
        //                                              r,
        //                                              0);

        G4LogicalVolume *logicNucleus = new G4LogicalVolume(solidNucleus,
                                                            waterMaterial,
                                                            "nucleus");

        G4PVPlacement *physiNucleus = new G4PVPlacement(0,
                                                        G4ThreeVector(R[r] * cos(theta), R[r] * sin(theta), z * micrometer),
                                                        logicNucleus,
                                                        "nucleus",
                                                        logicWorld,
                                                        0,
                                                        r,
                                                        0);
        numCells++;
      }
    }
    RunAction *myRunAction = (RunAction *)(G4RunManager::GetRunManager()->GetUserRunAction());
    myRunAction -> setNumCells(numCells);
  }
  logicWorld->SetVisAttributes(&invisGrey);
  logicSeed->SetVisAttributes(&visGrey);

  logicWorld->SetUserLimits(new G4UserLimits(0.25 * um));

  return physiWorld;
}
void DetectorConstruction::SetMin(G4double min)
{
  Rmin = min;
  RunAction *myRunAction = (RunAction *)(G4RunManager::GetRunManager()->GetUserRunAction());
  myRunAction->setRmin(min);
}
void DetectorConstruction::SetMax(G4double max)
{
  Rmax = max;
  RunAction *myRunAction = (RunAction *)(G4RunManager::GetRunManager()->GetUserRunAction());
  myRunAction->setRmax(max);
}
void DetectorConstruction::SetCells(G4double min, G4double max)
{
  for (G4int i = 0; i < R.size(); ++i)
  {
    R[i] = min + i * (max - min) / 9;
    G4cout << "R[" << i << "] = " << R[i] / um << G4endl;
  }
}