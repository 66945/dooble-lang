#pragma once

// package management
#ifndef DOOBLE_IMPL
#	define DOOBLE_EXTERNAL
#	ifdef DOOBLE_INTERNAL
#		error "internal.h cannot be included outside main package"
#	endif
#endif
