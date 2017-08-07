struct Settings{
  bool buttonDown;
  unsigned long buttonDownStartMs;
  bool stored;

  int pin;
  int bpm;
  int beatsPerBar;

  String name;
};

#define MIN_TEMPO 20
#define MAX_TEMPO 260
#define DEFAULT_TEMPO 100
#define DEFAULT_TS 4
