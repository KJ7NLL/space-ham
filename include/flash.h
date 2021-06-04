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

void flash_write(struct flash_entry **entries, uint32_t location);
void flash_read(struct flash_entry **entries, uint32_t location);
