# DO-178C-style verification of Paparazzi modules/safety/parachute.c
#
#   make test      build and run the requirements-based tests
#   make coverage  tests + statement, branch and MC/DC coverage reports
#   make trace     traceability matrix (HLR -> LLR -> test -> result)
#   make reqs      HTML requirements documents (StrictDoc)
#   make mutation  inject faults into a copy of the code, check tests catch them
#   make all       everything above
#
# PPRZ points at an unmodified clone of https://github.com/paparazzi/paparazzi

PPRZ   ?= ../paparazzi
CC     := gcc-14
GCOV   := gcov-14
BUILD  := build

SUT    := $(PPRZ)/sw/airborne/modules/safety/parachute.c

CFLAGS := -std=c99 -O0 -g -Wall -Wextra \
          -DUNITY_INCLUDE_DOUBLE \
          -Itest/stubs -Itest/unity/src \
          -I$(PPRZ)/sw/airborne -I$(PPRZ)/sw/include
COVFLAGS := --coverage -fcondition-coverage

.PHONY: all test coverage trace reqs mutation clean check-sut

all: coverage trace reqs

# Configuration management: refuse to run if the code under test is not the
# baselined version (Paparazzi commit 64176905c3cbdf592a13a9c2dd08f9c121b9d235).
check-sut:
	@test -f $(SUT) || (echo "ERROR: $(SUT) not found. Set PPRZ=<path to paparazzi clone>"; exit 1)
	@cd $(PPRZ) && sha256sum --quiet -c $(CURDIR)/baseline.sha256 \
	  || (echo "ERROR: parachute.c/.h differ from the verified baseline. Run: git -C $(PPRZ) checkout 64176905c3cbdf592a13a9c2dd08f9c121b9d235"; exit 1)
	@echo "Baseline OK: parachute.c and parachute.h match baseline.sha256"

$(BUILD):
	mkdir -p $(BUILD)

# Only the software under test is instrumented for coverage.
$(BUILD)/parachute.o: check-sut | $(BUILD)
	$(CC) $(CFLAGS) $(COVFLAGS) -c $(SUT) -o $@

$(BUILD)/unity.o: test/unity/src/unity.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_parachute.o: test/test_parachute.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/test_parachute: $(BUILD)/parachute.o $(BUILD)/unity.o $(BUILD)/test_parachute.o
	$(CC) --coverage $^ -lm -o $@

test: $(BUILD)/test_parachute
	rm -f $(BUILD)/*.gcda
	./$(BUILD)/test_parachute | tee $(BUILD)/test_results.txt

coverage: test
	$(GCOV) --conditions --branch-probabilities --branch-counts \
	    -o $(BUILD) $(BUILD)/parachute.o > $(BUILD)/gcov_summary.txt
	mv -f *.gcov $(BUILD)/
	cat $(BUILD)/gcov_summary.txt
	mkdir -p $(BUILD)/coverage
	gcovr --gcov-executable $(GCOV) --root $(PPRZ)/sw/airborne $(BUILD) \
	    --txt-metric line --txt-metric branch --txt-metric condition \
	    --txt $(BUILD)/coverage_summary.txt \
	    --html-details $(BUILD)/coverage/index.html \
	    --fail-under-line 100 --fail-under-branch 100 --fail-under-condition 100
	cat $(BUILD)/coverage_summary.txt

trace: test
	python3 scripts/trace_matrix.py reqs/HLR_parachute.sdoc reqs/LLR_parachute.sdoc \
	    test/test_parachute.c $(BUILD)/test_results.txt > $(BUILD)/traceability_matrix.md
	cat $(BUILD)/traceability_matrix.md

mutation: check-sut
	bash scripts/mutation_check.sh $(PPRZ)

reqs:
	strictdoc export reqs --output-dir $(BUILD)/reqs_html

clean:
	rm -rf $(BUILD)
