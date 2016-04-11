btsch: btsch.c
	$(CC) -o $@ $?

.PHONY: debug
debug: btsch.c
	$(RM) btsch_debug
	$(CC) -DBTSCH_DEBUG -o btsch_debug $?

.PHONY: clean
clean:
	$(RM) btsch btsch_debug btsch.o
