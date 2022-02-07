

const int MAX_RECEIVE_SIZE;

void setupCoprocessor(char *applicationId, const char *serialDeviceName);

void sendProgramToCoprocessor(char* program, char *output);

void callFunctionOnCoprocessor(char* functionName, char* parameters, char* output);

void callEvalOnCoprocessor(char* toEval, char* output);

void wait(float whatever);

char *strtokm(char *str, const char *delim);

OSErr closeSerialPort();