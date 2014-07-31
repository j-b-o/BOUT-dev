/*
 * MMS test of time integration methods
 * 
 * This code does very little, as all sources are added
 * in the solver code itself.
 */


#include <bout/physicsmodel.hxx>


class TimeTest : public PhysicsModel {
public:
  int init(bool restart) {
    solver->add(f, "f"); // Solve a single 3D field
    return 0;
  }
  int rhs(BoutReal time) {
    ddt(f) = 0.0; // No RHS here, only the MMS source
    return 0;
  }
private:
  Field3D f;      
};

BOUTMAIN(TimeTest);

