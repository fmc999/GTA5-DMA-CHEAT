#pragma once
#include <atomic>
#include <string>
#include <vector>

// Forward declare ImVec4 if needed
struct ImVec4;

struct SearchResult {
    uintptr_t address;
    uint32_t value;
    uint32_t previousValue; // Store previous value for comparison
    bool changed; // Flag to indicate if value has changed
    bool selected; // Flag to indicate if this result is selected
};

class PlayerChaser
{
public:
	// UI variables
	static inline bool bEnableUI = false;
	static inline char toolmanId[32] = { 0 };
	static inline char toolmanRid[32] = { 0 };
	static inline char targetId[32] = { 0 };
	static inline bool bSearchRequested = false;
	static inline bool bReplaceRequested = false;
	static inline bool bNextScanRequested = false; // Flag for next scan
	static inline char nextScanValue[32] = { 0 };
	static inline bool bSelectAllRequested = false; // Flag for select all operation
	static inline bool bDeselectAllRequested = false; // Flag for deselect all operation
	static inline bool bReplaceAllRequested = false; // Flag for replace all operation
	
	// Search results
	static inline std::vector<SearchResult> searchResults;
	static inline std::vector<SearchResult> firstScanResults; // Store first scan results
	static inline int selectedResultIndex = -1;
	static inline std::atomic<bool> searchInProgress = false; // Flag to indicate search is in progress
	static inline std::string statusMessage = "";
	static inline ImVec4 statusColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	
	// Atomic variables for thread safety
	static inline std::atomic<bool> bEnable = false;

	static void OnDMAFrame();
	static void UpdateChase();
	static void SyncUI();
	static void SearchForValue(const std::string& valueStr);
	static void NextScan(const std::string& valueStr);
	static bool ReplaceValue(uintptr_t address, const std::string& newValueStr);
	static void UpdateSearchResults();
	static void SelectAllResults();
	static void DeselectAllResults();
	static void ReplaceAllResults(const std::string& newValueStr);
	static void SetStatusMessage(const std::string& message, const ImVec4& color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
};