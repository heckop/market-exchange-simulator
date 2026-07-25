#include <iostream>
#include "bench/Histogram.hpp"
// TIP To <b>Run</b> code, press <shortcut actionId="Run"/> or click the <icon src="AllIcons.Actions.Execute"/> icon in the gutter.

int main() {
	// TIP Press <shortcut actionId="RenameElement"/> when your caret is at the <b>lang</b> variable name to see how CLion can help you rename it.

	const auto lang = "C++";
	std::cout << "Hello and welcome to " << lang << "!\n";
	Histogram h;
	for (int i = 1; i <= 100; i++) {
		// TIP Press <shortcut actionId="Debug"/> to start debugging your code. We have set one <icon src="AllIcons.Debugger.Db_set_breakpoint"/> breakpoint for you, but you can always add more by pressing <shortcut actionId="ToggleLineBreakpoint"/>.
		h.record(i*100);
	}
	std::cout<<"50th percentile latency "<<h.percentile(0.5)<<'\n';
	std::cout<<"99th percentile latency "<<h.percentile(0.99)<<'\n';
	std::cout<<"100th percentile latency "<<h.percentile(1)<<'\n';
	return 0;
	// TIP See CLion help at <a href="https://www.jetbrains.com/help/clion/">jetbrains.com/help/clion/</a>. Also, you can try interactive lessons for CLion by selecting 'Help | Learn IDE Features' from the main menu.
}