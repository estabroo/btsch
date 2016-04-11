SRC=btsch.c
VERSION=0.1
DIST_DIR=btsch-$(VERSION)

btsch: $(SRC)
	$(CC) -o $@ $?

.PHONY: debug
debug: $(SRC)
	$(RM) btsch_debug
	$(CC) -g -DBTSCH_DEBUG -o btsch_debug $?

.PHONY: clean
clean:
	$(RM) btsch btsch_debug btsch.o

dist:
	@$(RM) -rf $(DIST_DIR)
	@mkdir $(DIST_DIR)
	@cp -a $(SRC) $(DIST_DIR)
	@cp -a Makefile $(DIST_DIR)
	tar -czvf $(DIST_DIR).tar.gz $(DIST_DIR)
	sha1sum $(DIST_DIR).tar.gz > $(DIST_DIR).tar.gz.sha1sum
	@$(RM) -rf $(DIST_DIR)
