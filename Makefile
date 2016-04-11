SRC=btsch.c
VERSION=0.2
DIST_DIR=btsch-$(VERSION)

btsch: $(SRC)
	$(CC) -o $@ $?

btsch_debug: $(SRC)
	$(CC) -g -DBTSCH_DEBUG -o $@ $?

.PHONY: debug
debug: btsch_debug

.PHONY: clean
clean:
	$(RM) btsch btsch_debug btsch.o

.PHONY: dist
dist:
	@$(RM) -rf $(DIST_DIR)
	@mkdir $(DIST_DIR)
	@cp -a $(SRC) $(DIST_DIR)
	@cp -a Makefile Readme $(DIST_DIR)
	tar -czvf $(DIST_DIR).tar.gz $(DIST_DIR)
	sha1sum $(DIST_DIR).tar.gz > $(DIST_DIR).tar.gz.sha1sum
	@$(RM) -rf $(DIST_DIR)

.PHONY: dist_clean
dist_clean: clean
	$(RM) $(DIST_DIR).tar.gz $(DIST_DIR).tar.gz.sha1sum
