This project details a traffic light controller designed to be programmed onto an Altera DE2-115 development board.

Requirements:
- Altera DE2-115
- Nios 2 with ‘nios2_system.sopcinfo’ file
- Quartus with ‘cs303.sof’ file
- PuTTy or HyperTerminal

There are 4 mode configurations which are chosen using switches 0 and 1.
--> Mode 1 is independent whereas mode 2, 3 & 4 are an improvement of each other.

Mode 1: Simple traffic light controller.
Switch: All switches down
- 4 way intersection where traffic moves in the North-South and East-West directions only.
- Red Light lasts for 2 seconds, Green for 6 seconds, Yellow for 2 seconds.


Mode 2: Simple traffic light controller with Pedestrian Buttons.
Switch: Only switch 0 up
- Pedestrian buttons are simulated using key 0 (for North-South) and key 1 (for East-West) button presses on the board.


Mode 3: Configurable traffic lights
Switch: Only switch 1 up
- Timing length of each light can be changed by flip switch 2 up.
- The desired time in milli-seconds to be entered in PuTTy or HyperTerminal in the format:
#,#,#,#,#,# (where # is a 1 to 4 digit integer) and terminated with the enter/return key.
- Flip switch 2 back down after a successful input to resume execution with new timing values.


Mode 4: Simple traffic light controller with a Red light camera.
Switch: Only switch 0 and switch 1 up
- A car can be simulated using odd (car enter) and even (car exit) button presses of key 2.
- The red light camera activates when a car enters on a yellow light and does not leave before 2 seconds passes.
- The red light camera activates immediately when a car enters on a red light.
