# Makefile example for PrEp Plugin
HOME = /home/yahdzhyiev
SLURM_ROOT_DIR = /usr
SLURM_INC_DIR = $(SLURM_ROOT_DIR)/local/slurm/include/
SLURM_LIB_DIR = /usr/local/slurm/lib/slurm
SLURM_BUILD = $(SLURM_VERSION)
SLURM_BUILD_DIR = $(HOME)/slurm_build
SLURM_SRC_DIR = $(HOME)/src/slurm

EMA_INSTALL_DIR = /tmp/EMA
EMA_INCLUDE_DIR = $(EMA_INSTALL_DIR)/include
EMA_LIB_DIR = $(EMA_INSTALL_DIR)/lib

PLUGIN_TYPE = prep
PLUGIN_NAME = eps
PLUGIN_FILE = $(PLUGIN_TYPE)_$(PLUGIN_NAME).so

SRC_FILE = prep_eps.c

CC      = gcc
CFLAGS  ?= -Wall -fPIC -I$(SLURM_BUILD_DIR) -I$(SLURM_INC_DIR) -I$(SLURM_SRC_DIR) -I$(EMA_INCLUDE_DIR) -L$(EMA_LIB_DIR) -lEMA
LDFLAGS ?= -shared

all: $(PLUGIN_FILE)

test:
	gcc test.c -o test

default: $(PLUGIN_FILE)

$(PLUGIN_FILE): $(SRC_FILE)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

install: $(PLUGIN_FILE)
	install -m 755 $(PLUGIN_FILE) $(SLURM_LIB_DIR)

clean:
	rm -f $(PLUGIN_FILE)
	rm -f test

mrproper: clean
