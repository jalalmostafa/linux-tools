TARGETS := mem-diag

all: $(TARGETS)

$(TARGETS): %: %.c
	gcc $< -o $@

clean:
	rm $(TARGETS)
