# Makefile for EPS Plugins

SLURM_ROOT_DIR = /usr
SLURM_INC_DIR = $(SLURM_ROOT_DIR)/local/slurm/include/
SLURM_LIB_DIR = /usr/local/slurm/lib/slurm
SLURM_BUILD = $(SLURM_VERSION)

# !!! Edit this to be valid for your setup
SLURM_SRC_DIR = $(HOME)/src/slurm


EMA_DIR = /perfacct/slurm-libs/EMA
PQ_DIR = /perfacct/slurm-libs/postgresql

PLUGIN_TYPE = prep
PLUGIN_NAME = eps

PLUGINS_DIR = /perfacct/slurm-plugins

PLUGIN_FILE = $(PLUGIN_TYPE)_$(PLUGIN_NAME).so
SPANK_PLUGIN_FILE = $(PLUGIN_NAME).so

SRC_FILE = prep_eps.c
SPANK_SRC_FILE = eps.c

CC              = gcc
PREP_CFLAGS          ?= -Wall -fPIC -Iinclude \
                   -I$(SLURM_INC_DIR) -I$(SLURM_SRC_DIR)
SPANK_CFLAGS    ?= -Wall -fPIC -I$(SLURM_INC_DIR)
PREP_LDFLAGS         ?= -shared -L$(EMA_DIR)/lib -L$(PQ_DIR)/libs -lEMA -lpq
SPANK_LDFLAGS         ?= -shared

PREP_SRC_FILES = src/eps_resources.c src/eps_data.c src/eps_utils.c src/eps_db.c src/eps_ema.c
SPANK_SRC_FILES =

TESTS_DIR  = __test__

all: prep spank

prep: $(PLUGIN_FILE)

spank: $(SPANK_PLUGIN_FILE)

test:
	$(CC) -g $(TESTS_DIR)/basic.c -o basic
	$(CC) -g $(TESTS_DIR)/cgroup.c -o cgroup
	$(CC) -Iinclude -I$(SLURM_SRC_DIR) -g src/eps_utils.c src/eps_resources.c src/eps_data.c \
            src/eps_db.c $(TESTS_DIR)/db.c -L$(EMA_DIR)/lib -L$(PQ_DIR)/libs -lEMA -lpq -o dbtest
	$(CC) $(TESTS_DIR)/exit_failure.c -o fail
	$(CC) $(TESTS_DIR)/raise_sigsegv.c -o segf


default: $(PLUGIN_FILE)

$(PLUGIN_FILE): $(SRC_FILE)
	$(CC) $(PREP_CFLAGS) $(PREP_SRC_FILES) $^ $(PREP_LDFLAGS) -o $@

$(SPANK_PLUGIN_FILE): $(SPANK_SRC_FILE)
	$(CC) $(SPANK_CFLAGS) $(SPANK_SRC_FILES) $^ $(SPANK_LDFLAGS) -o $@

install: $(PLUGIN_FILE)
	install -m 755 $(PLUGIN_FILE) $(PLUGINS_DIR)
	install -m 755 $(SPANK_PLUGIN_FILE) $(PLUGINS_DIR)

clean:
	rm -f $(PLUGIN_FILE)
	rm -f $(SPANK_PLUGIN_FILE)
	rm -f basic cgroup dbtest fail segf

mrproper: clean
