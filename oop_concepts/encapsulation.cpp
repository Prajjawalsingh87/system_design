#include <iostream>
#include <string>

using namespace std;

/*
Encapsulation -->
1. Encapsulation means bundling the data (characteristics) and the methods (behaviour)
that work on that data into a single unit, i.e. a class.
2. It also means DATA HIDING: the internal state of an object is kept 'private' so that
the outside world cannot change it directly. The only way to read or modify it is
through the 'public' methods the class chooses to expose.
3. Because every change goes through those methods, the class can validate input and
keep its state consistent (e.g. speed can never be negative, gear can never be 7).

4. In our real world example of Car, the speed of the car is stored inside the
speedometer and the engine. You cannot open the dashboard and write a new number
into the speedometer. You can only change it through the controls the car gives you
(accelerate, brake). The car enforces its own rules.

5. Compare this with abstraction.cpp:
   - Abstraction hides the 'HOW' (implementation) behind an interface.
   - Encapsulation hides the 'DATA' (state) and protects it with rules.
*/
class SportsCar {
private:
    // Characteristics: hidden from the outside world.
    string brand;
    string model;
    bool isEngineOn;
    int currentSpeed;
    int currentGear;

    // Internal helper the outside world does not need to know about.
    bool isValidGear(int gear) const {
        return gear >= 0 && gear <= 6;
    }

public:
    SportsCar(string b, string m) {
        this->brand = b;
        this->model = m;
        isEngineOn = false;
        currentSpeed = 0;
        currentGear = 0;
    }

    // Getters: read-only access to the hidden data.
    string getBrand() const { return brand; }
    string getModel() const { return model; }
    int getSpeed() const { return currentSpeed; }
    int getGear() const { return currentGear; }

    // There is intentionally NO setSpeed(): speed can only change via accelerate()/brake().

    void startEngine() {
        isEngineOn = true;
        cout << brand << " " << model << " : Engine starts with a roar!" << endl;
    }

    // "Setter" with validation: invalid values are rejected instead of corrupting the state.
    void shiftGear(int gear) {
        if (!isEngineOn) {
            cout << brand << " " << model << " : Engine is off! Cannot Shift Gear." << endl;
            return;
        }
        if (!isValidGear(gear)) {
            cout << brand << " " << model << " : Invalid gear " << gear << "! Gear stays at " << currentGear << endl;
            return;
        }
        currentGear = gear;
        cout << brand << " " << model << " : Shifted to gear " << currentGear << endl;
    }

    void accelerate() {
        if (!isEngineOn) {
            cout << brand << " " << model << " : Engine is off! Cannot accelerate." << endl;
            return;
        }
        if (currentGear == 0) {
            cout << brand << " " << model << " : In neutral! Shift gear first." << endl;
            return;
        }
        currentSpeed += 20;
        cout << brand << " " << model << " : Accelerating to " << currentSpeed << " km/h" << endl;
    }

    void brake() {
        currentSpeed -= 20;
        if (currentSpeed < 0) currentSpeed = 0;
        cout << brand << " " << model << " : Braking! Speed is now " << currentSpeed << " km/h" << endl;
    }

    void stopEngine() {
        isEngineOn = false;
        currentGear = 0;
        currentSpeed = 0;
        cout << brand << " " << model << " : Engine turned off." << endl;
    }
};

// Main Method
int main() {

    SportsCar myCar("Ford", "Mustang");

    myCar.startEngine();
    myCar.accelerate();      // rejected: still in neutral
    myCar.shiftGear(1);
    myCar.accelerate();
    myCar.shiftGear(9);      // rejected: invalid gear, state stays consistent
    myCar.shiftGear(2);
    myCar.accelerate();
    myCar.brake();

    // myCar.currentSpeed = 500;   // Compile error: 'currentSpeed' is private.
    cout << "Current speed (via getter): " << myCar.getSpeed() << " km/h" << endl;

    myCar.stopEngine();

    return 0;
}
