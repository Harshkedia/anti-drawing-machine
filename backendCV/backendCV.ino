// POSITION FACTORS - DETERMINE THESE
#define X_DISTANCE_FACTOR 1.0
#define Y_DISTANCE_FACTOR 3.75

// SPEED TOLERANCE
#define SPEED_THRESHOLD_UPPER 40 
#define SPEED_THRESHOLD_LOWER 15

// LINEAR_TRANSITION_TOLERANCE
#define X_TOLERANCE 1.0
#define Y_TOLERANCE 1.0

// TIME BETWEEN FRAMES
#define FRAME_TIME 16.5 // these are in millisecond

// BAUD
#define BAUD_RATE 115200

// QUEUE SPECS
#define QUEUE_SIZE 3

#include <Pixy2.h>

Pixy2 pixy;

typedef struct {
  uint16_t x;
  uint16_t y;  
} PenPosition;

typedef struct {
  double vx;
  double vy;  
} PenVelocity;

typedef struct {
  PenPosition* pos;
  PenVelocity* vel;
  
  PenPosition* prevPositions[QUEUE_SIZE];
} PenMotion;

typedef struct {
  bool isHorizontal; // in X
  bool isVertical; // in Y
  bool isFast; // velocity magnitude
  bool isSlow; // velocity magnitude
  char dir; // direction N, E, S, W
} PenState;

PenPosition posInit = {0, 0};
PenVelocity velInit = {0.0, 0.0};
PenMotion penMotion = {&posInit, &velInit, {&posInit, &posInit, &posInit}};
PenState penState = {false, false, false, false, 'N'};
bool isSetup = false;
int setupCounter = 0;

void setup() {
  pixy.init();
  Serial.begin(BAUD_RATE);
}

/* PenState handlers */
void resetPenState() {
  penState = {false, false, false, false};
}

void decideHorizontal() {
  uint16_t firstPosition = penMotion.prevPositions[0]->x;
  bool isHorizontal = true;
  // check if all positions are in tolerance of the firstPosition
  for (int i = 0; i < QUEUE_SIZE; i += 1) {
    uint16_t currentPos = penMotion.prevPositions[i]->x;
    if (currentPos < firstPosition - X_TOLERANCE || 
        currentPos > firstPosition + X_TOLERANCE) {
      isHorizontal = false;
      break;  
    } 
  }
  penState.isHorizontal = isHorizontal;
}

void decideVertical() {
  uint16_t firstPosition = penMotion.prevPositions[0]->y;
  bool isVertical = true;
  // check if all positions are in tolerance of the firstPosition
  for (int i = 0; i < QUEUE_SIZE; i += 1) {
    uint16_t currentPos = penMotion.prevPositions[i]->y;
    if (currentPos < firstPosition - Y_TOLERANCE || 
        currentPos > firstPosition + Y_TOLERANCE) {
      isVertical = false;
      break;  
    } 
  }
  penState.isVertical = isVertical;
}

void decideFast() {
  double penSpeed = sqrt(penMotion.vel->vx * penMotion.vel->vx + penMotion.vel->vy * penMotion.vel->vy);

  penState.isFast = (penSpeed > SPEED_THRESHOLD_UPPER);
}

void decideSlow() {
  double penSpeed = sqrt(penMotion.vel->vx * penMotion.vel->vx + penMotion.vel->vy * penMotion.vel->vy);

  penState.isFast = (penSpeed < SPEED_THRESHOLD_LOWER);  
}

void decideDirection() {
  double mag = sqrt(penMotion.vel->vx * penMotion.vel->vx + penMotion.vel->vy * penMotion.vel->vy);

  double vy_abs = abs(penMotion.vel->vy);
  double vx_abs = abs(penMotion.vel->vx);

  double angle = atan(vy_abs / vx_abs);

  if (penMotion.vel->vx > 0 && penMotion.vel->vy > 0) {
    // NE  
    if (angle < PI / 4) {
      penState.dir = 'E';
    } else {
        penState.dir = 'N';
    }
  } else if (penMotion.vel->vx > 0 && penMotion.vel->vy < 0) {
    // SE
    if (angle < PI / 4) {
      penState.dir = 'E';
    } else {
        penState.dir = 'S';
    }
  } else if (penMotion.vel->vx < 0 && penMotion.vel->vy > 0) {
    // NW
    if (angle < PI / 4) {
      penState.dir = 'W';
    } else {
        penState.dir = 'N';
    }
  } else {
    // SW
    if (angle < PI / 4) {
      penState.dir = 'W';
    } else {
        penState.dir = 'S';
    }
  }
}

void frameListSetup(uint16_t x, uint16_t y) {
  if (setupCounter < QUEUE_SIZE - 1) {
    penMotion.prevPositions[setupCounter]->x = x;
    penMotion.prevPositions[setupCounter]->y = y;
    setupCounter += 1; 
  } else {
      isSetup = true;  
  }
}

/*
 * addBlockToFrameList: adds block to frame list
 */
void addBlockToFrameList(uint16_t x, uint16_t y) {
    for (int i = 0; i < QUEUE_SIZE - 1; i += 1) {
      penMotion.prevPositions[i]->x = penMotion.prevPositions[i+1]->x;
      penMotion.prevPositions[i]->y = penMotion.prevPositions[i+1]->y;
    }

    penMotion.prevPositions[QUEUE_SIZE - 1]->x = x;
    penMotion.prevPositions[QUEUE_SIZE - 1]->y = y; 
}

void updatePenPosition() {
  uint16_t x = penMotion.prevPositions[QUEUE_SIZE - 1]->x;
  uint16_t y = penMotion.prevPositions[QUEUE_SIZE - 1]->y;

  penMotion.pos->x = x;
  penMotion.pos->y = y;
}

void updatePenVelocities() {
  uint16_t mostRecentX = penMotion.prevPositions[QUEUE_SIZE - 1]->x;
  uint16_t mostRecentY = penMotion.prevPositions[QUEUE_SIZE - 1]->y;

  double velocitySumX = 0.0;
  double velocitySumY = 0.0;

  // get average of all velocities
  for (int i = QUEUE_SIZE - 2; i >= 0; i -= 1) {
    uint16_t x = penMotion.prevPositions[i]->x;
    uint16_t y = penMotion.prevPositions[i]->y; 

    double velocityX = (mostRecentX * 1.0 - x) / (QUEUE_SIZE - 1 - i);
    double velocityY = (mostRecentY * 1.0 - y) / (QUEUE_SIZE - 1 - i);

    velocitySumX += velocityX;
    velocitySumY += velocityY;
  }

  penMotion.vel->vx = velocitySumX / (QUEUE_SIZE - 1);
  penMotion.vel->vy = velocitySumY / (QUEUE_SIZE - 1);
}

void decideMove() {
  resetPenState();

  decideHorizontal();
  decideVertical();
  decideFast();
  decideSlow();
  decideDirection();

  if (penState.isHorizontal == true) {
    Serial.println("Pen is horizontal");  
  } 

  if (penState.isVertical == true) {
    Serial.println("Pen is vertical");  
  } 

  if (penState.isFast == true) {
    Serial.println("Pen is fast");  
  }

  if (penState.isSlow == true) {
    Serial.println("Pen is slow");  
  }

  // pen direction
  Serial.print("Pen direction is: ");
  Serial.println(penState.dir);

  switch (penState.dir) {
    case 'N':
      // CHECK FOR LINEARITY
      if (penState.isHorizontal) {
        // turn or jiggle  
      } else if (penState.isVertical) {
        // turn or jiggle  
      }

      // CHECK FOR SPEED
      if (penState.isFast) {
        // slow down
      } else if (penState.isSlow) {
        // speed up
      }
    break;
    case 'E':
      if (penState.isHorizontal) {
        // turn or jiggle  
      } else if (penState.isVertical) {
        // turn or jiggle  
      }
  
      // CHECK FOR SPEED
      if (penState.isFast) {
        // slow down
      } else if (penState.isSlow) {
        // speed up
      }
    break;
    case 'S':
      if (penState.isHorizontal) {
        // turn or jiggle  
      } else if (penState.isVertical) {
        // turn or jiggle  
      }

      // CHECK FOR SPEED
      if (penState.isFast) {
        // slow down
      } else if (penState.isSlow) {
        // speed up
      }
    break;
    case 'W':
      if (penState.isHorizontal) {
        // turn or jiggle  
      } else if (penState.isVertical) {
        // turn or jiggle  
      }

      // CHECK FOR SPEED
      if (penState.isFast) {
        // slow down
      } else if (penState.isSlow) {
        // speed up
      }
    break;  
    default:
      Serial.println("decideMove: direction not recognized");
  }
}

void printFrameList() {
  for (int i = QUEUE_SIZE - 1; i >= 0; i -= 1) {
    Serial.print("Frame #: ");
    Serial.println(i + 1);

    Serial.print("X position: ");
    Serial.println(penMotion.prevPositions[i]->x);
    Serial.print("Y position: ");
    Serial.println(penMotion.prevPositions[i]->y);
  }  
}

void loop() {
//  Serial.println(penMotion.pos->x);
  //Serial.println("Getting blocks");
  pixy.ccc.getBlocks();
  //Serial.println("Got blocks");
  // If there are detect blocks, print them!
  if (pixy.ccc.numBlocks) {
    uint16_t x = pixy.ccc.blocks[0].m_x;
    uint16_t y = pixy.ccc.blocks[0].m_y;

    if (!isSetup) {
      Serial.begin("Setting up");
      frameListSetup(x, y);  
    } else {
        Serial.begin("Set up complete");
        addBlockToFrameList(x, y);

        //printFrameList();
  
        updatePenPosition();
      
        updatePenVelocities();
  
        decideMove();  
    }

    Serial.print("X value = ");
    Serial.println(pixy.ccc.blocks[0].m_x); // only take most relevant block
    Serial.print("Y value = ");
    Serial.println(pixy.ccc.blocks[0].m_y);
  }  
}
