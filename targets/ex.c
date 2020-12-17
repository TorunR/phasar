int parse_unit(char*);
int unit;
void* axis_command;
void* parse_axis_command(char*);
void move_x(void*);
void move_y(void*);
void move_z(void*);
void home_axes();
#define NULL 0
#define UNSUPPORTED_COMMAND ""
#define FAIL(error) "";
#define FOO ""

void parse_command(char* input) {
  axis_command = NULL;
  int test2 =0;
  if (input[0] == 'G') {
    unit = parse_unit(input); // mm or inches
    axis_command = parse_axis_command(input);
    move_x(axis_command);
    move_y(axis_command);
#ifdef Z_ENABLED
    move_z(axis_command);
#endif
  } else if (input[0] == 'H'){
    coolant();
  } else { FAIL(UNSUPPORTED_COMMAND)  }
  int test = 0;
  test = getTest();
  if (axis_command) {
    do_command(test,test2);
  }
}

int getTest(int a, int b){
  int i = 0;
  int j = 2;
  return b;
}


// The Slice should look like this:

  void parse_command(char* input) {
    axis_command = NULL;
    if (input[0] == 'G') {
      axis_command = parse_axis_command(input);
      move_x(axis_command);
      move_y(axis_command);
    } else if (input[0] == 'H'){
    } else { }
    if (axis_command) {
      FOO;
    }
  }

  void parse_command(char* input) {
    axis_command = NULL;
    if (input[0] == 'G') {
      unit = parse_unit(input); // mm or inches
      axis_command = parse_axis_command(input);
      move_x(axis_command);
      move_y(axis_command);
  #ifdef Z_ENABLED
      move_z(axis_command);
  #endif
    } else if (input[0] == 'H'){
      coolant();
    } else { FAIL(UNSUPPORTED_COMMAND)  }
    if (axis_command) {}
  }