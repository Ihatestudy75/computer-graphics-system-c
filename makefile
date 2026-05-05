# Compiler and flags
CC ?= gcc
PROJECT_ROOT ?= ../..

INCLUDE_DIR ?= $(PROJECT_ROOT)/include
LIB_DIR ?= $(PROJECT_ROOT)/lib
BIN_DIR ?= $(PROJECT_ROOT)/bin/P10
OBJ_DIR ?= $(PROJECT_ROOT)/obj
P10_OBJ_DIR ?= $(OBJ_DIR)/P10
TEST_DIR ?= .

CFLAGS ?= -Wall -Wno-gnu-folding-constant -g -O2 \
	-I$(INCLUDE_DIR) \
	-I$(INCLUDE_DIR)/core \
	-I$(INCLUDE_DIR)/geometry \
	-I$(INCLUDE_DIR)/math \
	-I$(INCLUDE_DIR)/render \
	-I$(INCLUDE_DIR)/animation \
	-I$(INCLUDE_DIR)/texture
MKDIR_P ?= mkdir -p

PROGRAMS = demo_phong_vs demo_texture demo_particleSystem
TARGETS = $(addprefix $(BIN_DIR)/,$(PROGRAMS))

LIB_OBJS = \
	$(OBJ_DIR)/color.o \
	$(OBJ_DIR)/image.o \
	$(OBJ_DIR)/primitive.o \
	$(OBJ_DIR)/polygon.o \
	$(OBJ_DIR)/matrix.o \
	$(OBJ_DIR)/view.o \
	$(OBJ_DIR)/raster.o \
	$(OBJ_DIR)/module.o \
	$(OBJ_DIR)/bezier.o \
	$(OBJ_DIR)/light.o \
	$(OBJ_DIR)/particle.o \
	$(OBJ_DIR)/noise.o

.PHONY: all clean demo_phong_vs demo_texture demo_particleSystem runPhongVs runTexture runParticleSystem
.SECONDARY: $(addprefix $(P10_OBJ_DIR)/,$(addsuffix .o,$(PROGRAMS)))

all: $(TARGETS)

demo_phong_vs: $(BIN_DIR)/demo_phong_vs
demo_texture: $(BIN_DIR)/demo_texture
demo_particleSystem: $(BIN_DIR)/demo_particleSystem

$(BIN_DIR) $(OBJ_DIR) $(P10_OBJ_DIR):
	$(MKDIR_P) $@

$(BIN_DIR)/demo_phong_vs: $(P10_OBJ_DIR)/demo_phong_vs.o $(LIB_OBJS)
	$(MKDIR_P) $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN_DIR)/demo_texture: $(P10_OBJ_DIR)/demo_texture.o $(LIB_OBJS)
	$(MKDIR_P) $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BIN_DIR)/demo_particleSystem: $(P10_OBJ_DIR)/demo_particleSystem.o $(LIB_OBJS)
	$(MKDIR_P) $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(P10_OBJ_DIR)/demo_phong_vs.o: $(TEST_DIR)/demo_phong_vs.c
	$(MKDIR_P) $(P10_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(P10_OBJ_DIR)/demo_texture.o: $(TEST_DIR)/demo_texture.c
	$(MKDIR_P) $(P10_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(P10_OBJ_DIR)/demo_particleSystem.o: $(TEST_DIR)/demo_particleSystem.c
	$(MKDIR_P) $(P10_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/color.o: $(LIB_DIR)/core/color.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/image.o: $(LIB_DIR)/core/image.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/primitive.o: $(LIB_DIR)/geometry/primitive.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/polygon.o: $(LIB_DIR)/geometry/polygon.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/matrix.o: $(LIB_DIR)/math/matrix.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/view.o: $(LIB_DIR)/math/view.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/raster.o: $(LIB_DIR)/render/raster.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/module.o: $(LIB_DIR)/render/module.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/bezier.o: $(LIB_DIR)/geometry/bezier.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/light.o: $(LIB_DIR)/render/light.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/particle.o: $(LIB_DIR)/animation/particle.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/noise.o: $(LIB_DIR)/texture/noise.c
	$(MKDIR_P) $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

runPhongVs: $(BIN_DIR)/demo_phong_vs
	cd $(BIN_DIR) && ./demo_phong_vs

runTexture: $(BIN_DIR)/demo_texture
	cd $(BIN_DIR) && ./demo_texture

runParticleSystem: $(BIN_DIR)/demo_particleSystem
	cd $(BIN_DIR) && ./demo_particleSystem

clean:
	rm -f $(addprefix $(P10_OBJ_DIR)/,$(addsuffix .o,$(PROGRAMS))) $(TARGETS)
	rm -f $(BIN_DIR)/demo_flat.ppm $(BIN_DIR)/demo_gouraud.ppm $(BIN_DIR)/demo_phong.ppm
	rm -f $(BIN_DIR)/demo_flat_gouraud_phong.ppm $(BIN_DIR)/demo_phong_vs_gouraud.ppm
	rm -f $(BIN_DIR)/tex_wood.ppm $(BIN_DIR)/tex_marble.ppm
	rm -f $(BIN_DIR)/tex_scene1.ppm $(BIN_DIR)/tex_scene2.ppm $(BIN_DIR)/tex_scene3_*.ppm
	rm -f $(BIN_DIR)/ps_fire_*.ppm $(BIN_DIR)/ps_explosion_*.ppm $(BIN_DIR)/ps_complex_*.ppm
	rm -f $(BIN_DIR)/ps_fire.gif $(BIN_DIR)/ps_explosion.gif $(BIN_DIR)/ps_complex.gif
