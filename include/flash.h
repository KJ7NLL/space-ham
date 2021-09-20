#define FLASH_DATA_BASE (1024*128)
#define FLASH_FAT_BASE (FLASH_DATA_BASE + 1024*16)
#define FLASH_FAT_SIZE (FLASH_SIZE - FLASH_FAT_BASE)
#define FLASH_FAT_LBA_SIZE 512
#define FLASH_FAT_LBA_COUNT (FLASH_FAT_SIZE/FLASH_FAT_LBA_SIZE)

struct flash_header
{
	uint32_t len;
	uint32_t csum;
};

struct flash_entry
{
	char *name;
	void *ptr;
	uint32_t len;
};

int flash_write(struct flash_entry **entries, uint32_t location);
int flash_read(struct flash_entry **entries, uint32_t location);
char *flash_status(int status);
void flash_init();
