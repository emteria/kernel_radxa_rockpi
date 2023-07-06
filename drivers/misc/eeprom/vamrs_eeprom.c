#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_data/at24.h>
#include "vamrs_eeprom.h"

#define EERPOM_DEBUG	1

#if EERPOM_DEBUG
	#define DBG(args...) printk(args)
#else
	#define DBG(args...)
#endif

#define HEADER_SIGN 0x69502d52
#define CRC16 0x8005

char vamrs_mac_address[8][6] = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},};
char *vamrs_sn_number = NULL;


uint16_t getcrc(char* data, unsigned int size) {

    uint16_t out = 0;
    int bits_read = 0, bit_flag;

    /* Sanity check: */
    if((data == NULL) || size==0)
        return 0;

    while(size > 0)
    {
        bit_flag = out >> 15;

        /* Get next bit: */
        out <<= 1;
        out |= (*data >> bits_read) & 1; // item a) work from the least significant bits

        /* Increment bit counter: */
        bits_read++;
        if(bits_read > 7)
        {
            bits_read = 0;
            data++;
            size--;
        }

        /* Cycle check: */
        if(bit_flag)
            out ^= CRC16;

    }

    // item b) "push out" the last 16 bits
    int i;
    for (i = 0; i < 16; ++i) {
        bit_flag = out >> 15;
        out <<= 1;
        if(bit_flag)
            out ^= CRC16;
    }

    // item c) reverse the bits
    uint16_t crc = 0;
    i = 0x8000;
    int j = 0x0001;
    for (; i != 0; i >>=1, j <<= 1) {
        if (i & out) crc |= j;
    }

    return crc;
}


static const char* typeToAtomString(uint16_t type){
    switch (type){
        case ATOM_VENDOR_TYPE:
            return "Vendor";
        case ATOM_GPIO_TYPE:
            return "Gpio";
        case ATOM_DT_TYPE:
            return "Dtb";
        case ATOM_CUSTOM_TYPE:
            return "Custom";
        case ATOM_INVALID_TYPE:
            return "Invalid";
        default:
            return "Hinvalid";
    }
}

void static custom_parse(struct atom_t* atom){
	int i;
    if(!atom->data)
		return;

    VamrsCustom* custom = (VamrsCustom*) atom->data;

    DBG("sn   : %s\n", custom->sn);
    vamrs_sn_number = custom->sn;
    for(i = 0; i < (int) (sizeof(custom->mac)/sizeof(custom->mac[0])); i ++){
        DBG("mac%d : ", i+1);
        for(int j = 0; j < 6; j++){
			vamrs_mac_address[i][j] = custom->mac[i][j];
        }
    }
    /*
    DBG("test :\n");
	for(i = 0; i < sizeof(custom->test); i++){
        for(uint8_t j = 0; j < 8; j++){
            DBG("%d", (uint8_t)(custom->test[i] >> j) & 0x1u);
        }
        if((i + 1) % 4 == 0){
            DBG("\n");
        } else{
            DBG(" ");
        }
    }

    DBG("\n");
    */
}


static  EEPROM_RESULT readAtom(char** data, struct atom_t* atom){
    char* buffer = *data;
    memcpy(atom, buffer, ATOM_SIZE - CRC_SIZE);
    DBG("read eeprom %s atom size=0x%04x\n", typeToAtomString(atom->type), atom->dlen);

    uint32_t atomCrcOffset =  (ATOM_SIZE - CRC_SIZE) + (atom->dlen - CRC_SIZE);
    memcpy(&atom->crc16, buffer + atomCrcOffset, CRC_SIZE);
    uint16_t crc = getcrc(buffer, atomCrcOffset);
    DBG("read eeprom crc=0x%04x read=0x%04x\n", atom->crc16, crc);
    if(crc != atom->crc16) return EEPROM_ATOM_CRC_INVALID;

    void* atomData = kmalloc(atom->dlen - CRC_SIZE,GFP_KERNEL);
	if (!atomData) {
		DBG("%s atomData kmalloc fail \n",__func__);
        return -1;
    }

    memcpy(atomData, buffer + ATOM_SIZE - CRC_SIZE, atom->dlen - CRC_SIZE);
    atom->data = (char*)atomData;

    *data += ATOM_SIZE + atom->dlen - CRC_SIZE;

    return EEPROM_SUCCESS;
}


static int __init eeprom_custom_init(void)
{
	int i;
	int result;
	char * buffer,*data;
	uint32_t rom_data_len;
	ssize_t readSize;

	DBG("%s enter\n",__func__);

	VamrsEEPRom* rom = kmalloc(sizeof(*rom),GFP_KERNEL);
	if (!rom) {
		DBG("%s kmalloc fail \n",__func__);
        return -1;
    }
	memset(rom, 0, sizeof(*rom));

	readSize = at24_custom_read(0, &rom->header, HEADER_SIZE);
	if(readSize != HEADER_SIZE){
		DBG("%s read eeprom header error\n",__func__);
		return EEPROM_READ_ERROR;
    }

	if(rom->header.signature != HEADER_SIGN) {
		DBG("%s read eeprom header signature\n",__func__);
        return EEPROM_SIGN_ERROR;
    }

	DBG("read eeprom num atoms %d\n", rom->header.numatoms);
    if(rom->header.numatoms != ATOMS_NUM){
         return EEPROM_ATOM_NUM_INVALID;
    }

	DBG("read eeprom data %d\n", rom->header.eeplen);
	rom_data_len = rom->header.eeplen-HEADER_SIZE;
	buffer = kmalloc(rom_data_len, GFP_KERNEL);
	data = buffer;
	if (!buffer) {
		DBG("%s kmalloc fail \n",__func__);
        return -1;
    }
	memset(buffer, 0,rom_data_len);
	readSize = at24_custom_read(HEADER_SIZE, buffer, rom_data_len);

	for(i = 0; i < rom->header.numatoms; i++){
        result = readAtom(&buffer, &rom->atoms[i]);
        if(result){
             return result;
        }
    }

	custom_parse(&rom->atoms[ATOM_CUSTOM_TYPE-1]);

	DBG("read eeprom data sn = %s\n", vamrs_sn_number);
	DBG("read eeprom data mac0 = %02x:%02x:%02x:%02x:%02x:%02x\n", \
		vamrs_mac_address[0][1],vamrs_mac_address[0][2],vamrs_mac_address[0][3],vamrs_mac_address[0][4],vamrs_mac_address[0][5],vamrs_mac_address[0][6]);
	DBG("read eeprom data mac1 = %02x:%02x:%02x:%02x:%02x:%02x\n", \
		vamrs_mac_address[1][1],vamrs_mac_address[1][2],vamrs_mac_address[1][3],vamrs_mac_address[1][4],vamrs_mac_address[1][5],vamrs_mac_address[1][6]);

	kfree(rom);
	kfree(data);

    return 0;
}

static void __exit eeprom_custom_exit(void)
{
    // TODO: Cleanup and release any resources if necessary
}

late_initcall(eeprom_custom_init);
module_exit(eeprom_custom_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("liuxian@radxa.com");
MODULE_DESCRIPTION("Reading custom data from EEPROM");
