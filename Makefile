# Makefile example for PrEp Plugin

SLURM_ROOT_DIR = /usr
SLURM_INC_DIR = $(SLURM_ROOT_DIR)/local/slurm/include/
SLURM_LIB_DIR = /usr/local/slurm/lib/slurm
SLURM_BUILD = $(SLURM_VERSION)
SLURM_BUILD_DIR = $(HOME)/slurm_build
SLURM_SRC_DIR = $(HOME)/src/slurm

EMA_DIR = /perfacct/slurm-libs/EMA

PLUGIN_TYPE = prep
PLUGIN_NAME = eps
PLUGIN_FILE = $(PLUGIN_TYPE)_$(PLUGIN_NAME).so
SPANK_PLUGIN_FILE = $(PLUGIN_NAME).so

SRC_FILE = prep_eps.c
SPANK_SRC_FILE = eps.c

CC              = gcc
CFLAGS          ?= -Wall -fPIC \
                   -I$(SLURM_BUILD_DIR) -I$(SLURM_INC_DIR) -I$(SLURM_SRC_DIR)
SPANK_CFLAGS    ?= -Wall -fPIC -I$(SLURM_INC_DIR)
LDFLAGS         ?= -shared -L$(EMA_DIR)/lib -lEMA

all: prep spank

prep: $(PLUGIN_FILE)

spank: $(SPANK_PLUGIN_FILE)

test:
	gcc test.c -o test
	gcc testcg.c -o testcg

default: $(PLUGIN_FILE)

$(PLUGIN_FILE): $(SRC_FILE)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(SPANK_PLUGIN_FILE): $(SPANK_SRC_FILE)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

install: $(PLUGIN_FILE)
	install -m 755 $(PLUGIN_FILE) $(SLURM_LIB_DIR)

clean:
	rm -f $(PLUGIN_FILE)
	rm -f test testcg

mrproper: clean
