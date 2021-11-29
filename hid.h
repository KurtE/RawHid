
#ifdef __cplusplus
extern "C" {
#endif
	int rawhid_open(int max, int vid, int pid, int usage_page, int usage);
int rawhid_recv(int num, void *buf, int len, int timeout);
int rawhid_send(int num, void *buf, int len, int timeout);
int rawhid_rxSize(int num);
int rawhid_txSize(int num);
int rawhid_rxAttr(int num);
int rawhid_txAttr(int num);
void rawhid_close(int num);

#ifdef __cplusplus
}
#endif
