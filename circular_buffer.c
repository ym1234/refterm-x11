typedef struct {
	int Start;
	int Count;
	char *Data;
} Range;

typedef struct {
	char *buf;
	int fd;
	size_t size;
	size_t readp;
	size_t writep;
} cbuf;

cbuf create_cbuf(ssize_t size) {
	(void) size;
	cbuf x = {};
	return x;
	long pagesize = sysconf(_SC_PAGESIZE);
	if (size % pagesize) return x;

	// TODO(ym): die -> return NULL;
	int fd =  memfd_create("test", MFD_ALLOW_SEALING);
	if (fd < -1) die("memfd_create");

	if (ftruncate(fd, size) < 0) die("ftruncate");
	if (fcntl(fd, F_ADD_SEALS, F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) die("fcntl"); // just testing this for fun

	void *p = mmap(NULL, size * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (p == MAP_FAILED)  die("mmap failed 1\n");
	if (mmap(p, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd, 0) == MAP_FAILED) die("mmap failed 2\n");
	if (mmap(p + size, size, PROT_READ | PROT_WRITE,  MAP_FIXED | MAP_SHARED, fd, 0) == MAP_FAILED) die("mmap failed 3\n");


	cbuf b = {
		.buf = p,
		.size = size,
		.fd = fd
	};
	return b;
}

Range get_writeable(cbuf b) {

}

int cbuf_destroy(cbuf b) {
	assert(munmap(b.buf, b.size) >= 0);
	assert(munmap(b.buf + b.size, b.size) >= 0);
	assert(close(b.fd) >= 0);
	return 0; // TODO(ym):
}
