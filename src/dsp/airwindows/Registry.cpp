// The Airwindows factories. Written by tools/port_airwindows.py.

#include "Registry.h"

#include "ToTape6.h"
#include "IronOxide5.h"
#include "Tape.h"
#include "Density.h"
#include "Drive.h"
#include "Spiral2.h"
#include "PurestDrive.h"
#include "Tube2.h"
#include "Mojo.h"
#include "Coils2.h"
#include "Console7Channel.h"
#include "Console7Buss.h"
#include "Channel9.h"
#include "Air.h"
#include "Air3.h"
#include "Capacitor2.h"
#include "Baxandall2.h"
#include "Isolator2.h"
#include "Holt2.h"
#include "Pressure5.h"
#include "ButterComp2.h"
#include "Logical4.h"
#include "Pyewacket.h"
#include "Pop2.h"
#include "Galactic.h"
#include "kCathedral.h"
#include "Verbity2.h"
#include "Chamber.h"
#include "MatrixVerb.h"
#include "Wider.h"
#include "Srsly2.h"
#include "ToVinyl4.h"
#include "DeRez2.h"
#include "Deckwrecka.h"
#include "BitShiftGain.h"
#include "DrumSlam.h"
#include "Inflamer.h"
#include "Flutter.h"
#include "ChorusEnsemble.h"
#include "ToTape8.h"
#include "TapeHack2.h"
#include "Density3.h"
#include "TapeDelay2.h"
#include "PitchDelay.h"
#include "StarChild2.h"
#include "PurestEcho.h"
#include "Doublelay.h"
#include "Ensemble.h"
#include "Chorus.h"
#include "Vibrato.h"
#include "Desk4.h"
#include "TapeDust.h"
#include "Dirt.h"
#include "StereoFX.h"

namespace airwindows
{
std::unique_ptr<Algorithm> createToTape6() { return std::make_unique<ToTape6>(); }
std::unique_ptr<Algorithm> createIronOxide5() { return std::make_unique<IronOxide5>(); }
std::unique_ptr<Algorithm> createTape() { return std::make_unique<Tape>(); }
std::unique_ptr<Algorithm> createDensity() { return std::make_unique<Density>(); }
std::unique_ptr<Algorithm> createDrive() { return std::make_unique<Drive>(); }
std::unique_ptr<Algorithm> createSpiral2() { return std::make_unique<Spiral2>(); }
std::unique_ptr<Algorithm> createPurestDrive() { return std::make_unique<PurestDrive>(); }
std::unique_ptr<Algorithm> createTube2() { return std::make_unique<Tube2>(); }
std::unique_ptr<Algorithm> createMojo() { return std::make_unique<Mojo>(); }
std::unique_ptr<Algorithm> createCoils2() { return std::make_unique<Coils2>(); }
std::unique_ptr<Algorithm> createConsole7Channel() { return std::make_unique<Console7Channel>(); }
std::unique_ptr<Algorithm> createConsole7Buss() { return std::make_unique<Console7Buss>(); }
std::unique_ptr<Algorithm> createChannel9() { return std::make_unique<Channel9>(); }
std::unique_ptr<Algorithm> createAir() { return std::make_unique<Air>(); }
std::unique_ptr<Algorithm> createAir3() { return std::make_unique<Air3>(); }
std::unique_ptr<Algorithm> createCapacitor2() { return std::make_unique<Capacitor2>(); }
std::unique_ptr<Algorithm> createBaxandall2() { return std::make_unique<Baxandall2>(); }
std::unique_ptr<Algorithm> createIsolator2() { return std::make_unique<Isolator2>(); }
std::unique_ptr<Algorithm> createHolt2() { return std::make_unique<Holt2>(); }
std::unique_ptr<Algorithm> createPressure5() { return std::make_unique<Pressure5>(); }
std::unique_ptr<Algorithm> createButterComp2() { return std::make_unique<ButterComp2>(); }
std::unique_ptr<Algorithm> createLogical4() { return std::make_unique<Logical4>(); }
std::unique_ptr<Algorithm> createPyewacket() { return std::make_unique<Pyewacket>(); }
std::unique_ptr<Algorithm> createPop2() { return std::make_unique<Pop2>(); }
std::unique_ptr<Algorithm> createGalactic() { return std::make_unique<Galactic>(); }
std::unique_ptr<Algorithm> createkCathedral() { return std::make_unique<kCathedral>(); }
std::unique_ptr<Algorithm> createVerbity2() { return std::make_unique<Verbity2>(); }
std::unique_ptr<Algorithm> createChamber() { return std::make_unique<Chamber>(); }
std::unique_ptr<Algorithm> createMatrixVerb() { return std::make_unique<MatrixVerb>(); }
std::unique_ptr<Algorithm> createWider() { return std::make_unique<Wider>(); }
std::unique_ptr<Algorithm> createSrsly2() { return std::make_unique<Srsly2>(); }
std::unique_ptr<Algorithm> createToVinyl4() { return std::make_unique<ToVinyl4>(); }
std::unique_ptr<Algorithm> createDeRez2() { return std::make_unique<DeRez2>(); }
std::unique_ptr<Algorithm> createDeckwrecka() { return std::make_unique<Deckwrecka>(); }
std::unique_ptr<Algorithm> createBitShiftGain() { return std::make_unique<BitShiftGain>(); }
std::unique_ptr<Algorithm> createDrumSlam() { return std::make_unique<DrumSlam>(); }
std::unique_ptr<Algorithm> createInflamer() { return std::make_unique<Inflamer>(); }
std::unique_ptr<Algorithm> createFlutter() { return std::make_unique<Flutter>(); }
std::unique_ptr<Algorithm> createChorusEnsemble() { return std::make_unique<ChorusEnsemble>(); }
std::unique_ptr<Algorithm> createToTape8() { return std::make_unique<ToTape8>(); }
std::unique_ptr<Algorithm> createTapeHack2() { return std::make_unique<TapeHack2>(); }
std::unique_ptr<Algorithm> createDensity3() { return std::make_unique<Density3>(); }
std::unique_ptr<Algorithm> createTapeDelay2() { return std::make_unique<TapeDelay2>(); }
std::unique_ptr<Algorithm> createPitchDelay() { return std::make_unique<PitchDelay>(); }
std::unique_ptr<Algorithm> createStarChild2() { return std::make_unique<StarChild2>(); }
std::unique_ptr<Algorithm> createPurestEcho() { return std::make_unique<PurestEcho>(); }
std::unique_ptr<Algorithm> createDoublelay() { return std::make_unique<Doublelay>(); }
std::unique_ptr<Algorithm> createEnsemble() { return std::make_unique<Ensemble>(); }
std::unique_ptr<Algorithm> createChorus() { return std::make_unique<Chorus>(); }
std::unique_ptr<Algorithm> createVibrato() { return std::make_unique<Vibrato>(); }
std::unique_ptr<Algorithm> createDesk4() { return std::make_unique<Desk4>(); }
std::unique_ptr<Algorithm> createTapeDust() { return std::make_unique<TapeDust>(); }
std::unique_ptr<Algorithm> createDirt() { return std::make_unique<Dirt>(); }
std::unique_ptr<Algorithm> createStereoFX() { return std::make_unique<StereoFX>(); }
} // namespace airwindows
