#define AWFUL_ERROR do { fprintf(stderr, "Unrecoverable error at %s in %s:%d\n", __FUNCTION__, __FILE__, __LINE__); } while(0)
