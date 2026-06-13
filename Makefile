# ============================================
# EmotionOS Makefile (cross-platform)
# ============================================

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Iinclude -Isrc -O0
LDFLAGS  ?= -static

SRCDIR   := src
BUILDDIR := build

SRCS     := $(SRCDIR)/kernel.cpp $(SRCDIR)/main.cpp
OBJS     := $(SRCS:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
TARGET   := $(BUILDDIR)/emotionos

.PHONY: all debug release clean run

all: debug

debug: CXXFLAGS += -O0 -g
debug: $(TARGET)_dbg

release: CXXFLAGS += -O2 -s
release: $(TARGET)

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[OK] $(TARGET)"

$(TARGET)_dbg: $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[OK] $(TARGET)_dbg"

run: release
	./$(TARGET)

clean:
	rm -rf $(BUILDDIR)
	@echo "[CLEAN] done"
