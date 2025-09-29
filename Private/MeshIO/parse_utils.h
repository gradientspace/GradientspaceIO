// Copyright Gradientspace Corp. All Rights Reserved.
#pragma once

static constexpr char null_char = '\0';


inline bool is_line_space(char c) {
	return c == ' ' || c == '\t';
}
inline bool is_end_of_line(char c) {
	return c == '\r' || c == '\n' || c == null_char;
}
inline bool check_eol(const char* LineString, int index) { 
	return is_end_of_line(LineString[index]);
}




inline void trim_start_end_in_place(char*& String, int& N)
{
	if (String[N - 1] == '\n') {
		N--;
		String[N] = null_char;
	}
	if (String[N - 1] == '\r') {
		N--;
		String[N] = null_char;
	}
	// trim empty space
	while (is_line_space(String[0]) && N > 0)
	{
		String++;
		N--;
	}
}

