#ifndef	_VAMRS_EEPROM_H_
#define	_VAMRS_EEPROM_H_


#define ATOMS_NUM    4
#define VAMRS_SN_LEN  64

/* Atom types */
#define ATOM_INVALID_TYPE 	0x0000
#define ATOM_VENDOR_TYPE 0x0001
#define ATOM_GPIO_TYPE	0x0002
#define ATOM_DT_TYPE 		0x0003
#define ATOM_CUSTOM_TYPE		0x0004
#define ATOM_HINVALID_TYPE	0xffff

#define ATOM_VENDOR_NUM 0x0000
#define ATOM_GPIO_NUM 0x0001
#define ATOM_DT_NUM 0x0002

//minimal sizes of data structures
#define HEADER_SIZE 12
#define ATOM_SIZE 10
#define VENDOR_SIZE 22
#define GPIO_SIZE 30
#define CRC_SIZE 2

#define GPIO_MIN 2
#define GPIO_COUNT 28

#define FORMAT_VERSION 0x01

/* EEPROM header structure */
struct header_t {
	uint32_t signature;
	unsigned char ver;
	unsigned char res;
	uint16_t numatoms;
	uint32_t eeplen;
};

/* Atom structure */
struct atom_t {
	uint16_t type;
	uint16_t count;
	uint32_t dlen;
	char* data;
	uint16_t crc16;
};

/* Vendor info atom data */
struct vendor_info_d {
	uint32_t serial_1; //least significant
	uint32_t serial_2;
	uint32_t serial_3;
	uint32_t serial_4; //most significant
	uint16_t pid;
	uint16_t pver;
	unsigned char vslen;
	unsigned char pslen;
	char* vstr;
	char* pstr;
};

/* GPIO map atom data */
struct gpio_map_d {
	unsigned char flags;
	unsigned char power;
	unsigned char pins[GPIO_COUNT];
};




typedef enum{
    EEPROM_NOT_VALID = -11,
    EEPROM_INVALID_MAC = -11,
    EEPROM_INVALID_KEY_TYPE = -10,
    EEPROM_ATOM_CRC_INVALID = -9,
    EEPROM_ATOM_NUM_INVALID = -8,
    EEPROM_SIGN_ERROR = -7,
    EEPROM_INIT_ERROR = -6,
    EEPROM_DTB_READ_ERROR = -5,
    EEPROM_DTB_NOT_FOUND = -4,
    EEPROM_WRITE_ERROR = -3,
    EEPROM_READ_ERROR = -2,
    EEPROM_NOT_FOUND = -1,
    EEPROM_SUCCESS = 0,
}EEPROM_RESULT;

typedef enum{
    VAMRS_KEY_INVALID=0,
    VAMRS_SN = 1,
    VAMRS_MAC1,
    VAMRS_MAC2,
    VAMRS_MAC3,
    VAMRS_MAC4,
    VAMRS_MAC5,
    VAMRS_MAC6,
    VAMRS_MAC_WIFI,
    VAMRS_MAC_BT,
    VAMRS_TEST,
}KEY_TYPE;

/**
 * atoms[0] vendor
 * atoms[1] gpio
 * atoms[2] dtb
 * atoms[3] custom
 */
typedef struct{
    struct header_t  header;
    struct atom_t    atoms[ATOMS_NUM];
    uint8_t          i2c;
    uint8_t          i2c_addr;
    uint8_t          valid;
}VamrsEEPRom;

typedef struct{
    char     sn[VAMRS_SN_LEN];
    uint8_t  mac[8][6];
    uint8_t  test[16];
}VamrsCustom;



int           verifyMac(const uint8_t* mac);
void          stringMacToHex(const char* mac, uint8_t* data);
EEPROM_RESULT initEEPRom(VamrsEEPRom* rom, const char* dtboPath);
EEPROM_RESULT readEEPRom(VamrsEEPRom* rom);
EEPROM_RESULT getVamrsKey(VamrsEEPRom* rom, KEY_TYPE type, void* data, uint32_t size);
EEPROM_RESULT setVamrsKey(VamrsEEPRom* rom, KEY_TYPE type, void* data, uint32_t size);
void          dumpEEPRom(VamrsEEPRom* rom);
EEPROM_RESULT doWrite(VamrsEEPRom* rom, const char* path);
EEPROM_RESULT doWriteToEEPRom(VamrsEEPRom* rom);
void          destroy(VamrsEEPRom* rom);




#endif
