#include <stdint.h>

class Wire {
public:
    Wire() {};

    void begin(void);

    void beginTransmission(int dev_addr);
    void write(uint8_t reg);
    void endTransmission(void);

    void requestFrom(uint8_t dev_addr, uint8_t len);
    uint8_t available(void);
    uint8_t read(void);

private:
    uint8_t write_dev_addr;
    uint8_t read_buff_len;
    uint8_t read_buff_idx;
    uint8_t write_buff_len;
    uint8_t write_buff[32];
    uint8_t read_buff[32];
};

extern class Wire Wire;
