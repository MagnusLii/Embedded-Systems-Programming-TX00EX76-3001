**Unipolar stepper motor control**<br><br>

**About stepper motors**<br><br>

Stepper motors are brushless motors that when driven with DC turn in steps. A traditional brushed motor
will turn constantly when powered with DC. When a stepper motor coil is energized the motor turns just a
bit (one step) and then holds it position provided that the coil remains energized. Stepper motor has
multiple coils (wingdings) and by energizing the coils in a sequence the motor can be turned in the desired
direction wanted number of steps or turns. The step size the motor takes depends on the way the coils are
wound. The number of steps per revolution is usually 16 – 400. A stepper motor can be driven to a certain
position in a consistent and repeatable way by counting the number of steps taken. For example, if the
motor is stepped 16 times clockwise then to return to the original position, we need to take 16 steps
counter-clockwise.<br><br>
There are two types of stepper motors: unipolar and bipolar.<br><br>
* Unipolar stepper motor is 4-phase brushless motor with four coils and 5 or 6 wires.
* Bipolar stepper motor has two coils and four wires – two wires for each coil.<br><br>
The stepper motor used in this exercise is 28BYJ-48. It is a unipolar stepper motor with 32 steps per
revolution and 1:64 reduction gearing which means that there are 64 x 32 = 2048 steps per revolution. In
this exercise we will use half stepping which will increase the steps per revolution to 4096 steps/rev.<br><br>
The operating principle of a unipolar stepper motor is shown below. The diagram shows simplified coil
winding to illustrate the operating principle. The winding and the internal mechanics are more complex in a
real stepper motor.<br><br>
There are 4 coils: A, B, C and D. Each coil has one dedicated
wire and the other end of each coil is wired to a common
point (the 5th wire in the motor). The common wire is
connected to the positive terminal of Motor power supply.
The other wires are connected to a motor driver.<br><br>
The motor driver energizes a coil by connecting the coil to
ground which makes the current flow though the coil. Our
driver board has a ULN2003A which is low-side driver chip.<br><br>
A low-side driver is placed between the load and ground
which is exactly what we need in this case.<br><br>

**Stepper motor driving sequence**<br><br>

When coil A is energized the rotor turns towards the energized coil.<br><br>
Next coil B is energized to turn rotor one step further<br><br>
Then coil C<br><br>
And coil D<br><br>
Then coil A again, etc.<br><br>
The driver board has inputs IN1 – IN4 they are wired to the stepper motor coils: A → IN1, B → IN2, etc. The
table below show the driving sequence. To drive motor in the opposite direction the sequence is run
backwards (steps: 1, 4, 3, 2, 1, …)<br><br>
Step A B C D<br>
1 1 0 0 0<br>
2 0 1 0 0<br>
3 0 0 1 0<br>
4 0 0 0 1<br><br>
A unipolar stepper motor can also be driven by energizing two coils at the same time which gives better
torque for the motor.<br><br>
Energize coils A and B<br><br>
Then B and C<br><br>
And C and D<br><br>
Finally, A and D<br><br>
Sequence starts over.<br><br>
Driving sequence for dual coil drive:<br>
Step A B C D<br>
1 1 1 0 0<br>
2 0 1 1 0<br>
3 0 0 1 1<br>
4 1 0 0 1<br><br>

**Half stepping**<br><br>
By combining to the two sequences, we get half stepping which increases accuracy by dividing each step by
two. With half stepping we have 64 steps per revolution which makes 4096 steps per revolution with 1:64
gearing.<br><br>
The steps can be divided into smaller fractions by using micro stepping which allows motors to run
smoother and more accurately than in a standard setup. Micro stepping requires a driver chip to handle
stepping and also requires bipolar stepper motor. The driver chip keeps track of where in the sequence the
motor is at and is typically controlled by setting direction and then pulsing the step input to advance in the
stepping sequence.<br><br>
Driving sequence for half stepping.<br><br>
Step A B C D<br>
1 1 0 0 0<br>
2 1 1 0 0<br>
3 0 1 0 0<br>
4 0 1 1 0<br>
5 0 0 1 0<br>
6 0 0 1 1<br>
7 0 0 0 1<br>
8 1 0 0 1<br><br>

**Stepper motor calibration**<br><br>
It is essential to follow the stepping sequence when driving the motor. It is not possible to skip steps
because the internal structure of the stepper motor does not allow the motor to turn more than one step
at a time. This means that when you start driving a stepper motor after power on you don’t know the
position of the shaft and the position in the sequence where to start.<br><br>
You can start running the motor from any point in the sequence but it may take up the length of the
sequence before the motor “catches” the sequence and starts turning. Therefore, a stepper motor requires
position calibration at startup. Once the motor position is calibrated it can be controlled precisely.
A common way to calibrate position is to run the stepper motor in one direction until a limit switch is
activated. The switch can be mechanical, optical, or magnetic. When the switch is activated, the position of
the motor is known both in terms of physical location and position in the stepping sequence. When the
motor is stopped the stepping needs to continue from the next step when the motor is run again. For
example, if the motor was stopped at step 5 in the sequence, then the next step is either 6 or 4 depending
on which direction the motor is to be turned.<br><br>
A stepper motor is stopped by leaving the coil(s) of the current step energized which makes the motor to
hold its position with a holding torque that depends on the motor.<br><br>
It is also possible to switch all the coils off but then the holding torque is lost and the mechanics of the
device determine whether the position is held or not. For example, the build plate of a 3D-printer does not
move in X/Y direction even when it is powered off when the printer is level on a table.<br><br>

**Stepper motor speed**<br><br>
Stepper motor can be driven very slowly. There is no upper limit for time between the steps but there is
lower limit for time between step i.e., there is an upper limit for stepper RPM. The motor speed is limited
by the internal structure of the motor and you can (sometimes) find that in the motor datasheet. The
maximum speed can’t be reached immediately – it requires that motor is accelerated by starting with a
lower speed and then increasing the speed gradually until the maximum speed is reached. When motor is
running at high speed stopping should also be done gradually to ensure that position accuracy is
maintained. Especially if the mass that motor is moving large compared to the holding torque of the motor
the motor may “slip” if stopped without decelerating.<br><br>

**Exercise 1**<br><br>
In this exercise you need to calibrate the stepper motor position and count the number of steps per
revolution by using an optical sensor. The reducer gearing ratio is not exactly 1:64 so the number of steps
per revolution is not exactly 4096 steps with half-stepping. By calibrating motor position and the number of
steps per revolution we can get better accuracy.<br><br>
Connect the stepper motor driver board to the 6x1 pin connector on the development board so that motor
connector is facing up. Connect motor connector to driver board and the dispenser base grove cable to
ADC_1 connector.<br><br>
* Opto fork – GP28 – Configure as an input with pull-up
* Stepper motor controller – GP2, GP3, GP6 and GP13
* All four pins are outputs
* Pins are connected to the stepper motor driver pins IN1 – IN4<br><br>
Implement a program that reads commands from standard input. The commands to implement are:
**status** – prints the state of the system:<br>
* Is it calibrated?
* Number of steps per revolution or “not available” if not calibrated
**calib** – perform calibration<br>
* Calibration should run the motor in one direction until a falling edge is seen in the opto fork
input and then count number of steps to the next falling edge. To get more accurate results
count the number of steps per revolution three times and take the average of the values.
**run N** – N is an integer that may be omitted. Runs the motor N times 1/8th of a revolution. If N is
omitted run one full revolution. “Run 8” should also run one full revolution.