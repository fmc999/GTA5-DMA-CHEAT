#include "pch.h"
#include "PlayerChaser.h"
#include "DMA.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

void PlayerChaser::OnDMAFrame()
{
	// Sync UI values to atomic values
	SyncUI();
	
	// Update chase state
	UpdateChase();
	
	// Update search results with current values
	UpdateSearchResults();
}

void PlayerChaser::UpdateChase()
{
	if (bSearchRequested) {
		// Perform the search
		if (strlen(toolmanRid) > 0) {
			searchInProgress = true;
			SetStatusMessage("开始搜索...", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
			SearchForValue(std::string(toolmanRid));
			searchInProgress = false;
			SetStatusMessage("搜索完成，找到 " + std::to_string(searchResults.size()) + " 个结果", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		} else {
			SetStatusMessage("请输入RID值", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
		bSearchRequested = false;
	}
	
	if (bNextScanRequested) {
		// Perform next scan
		if (strlen(nextScanValue) > 0) {
			searchInProgress = true;
			SetStatusMessage("开始下一次扫描...", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
			NextScan(std::string(nextScanValue));
			searchInProgress = false;
			SetStatusMessage("扫描完成，找到 " + std::to_string(searchResults.size()) + " 个结果", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		} else {
			NextScan(""); // Update values only
			SetStatusMessage("已更新结果值", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		}
		bNextScanRequested = false;
	}
	
	if (bReplaceRequested && selectedResultIndex >= 0 && selectedResultIndex < (int)searchResults.size()) {
		// Perform the replacement
		if (strlen(targetId) > 0) {
			bool success = ReplaceValue(searchResults[selectedResultIndex].address, std::string(targetId));
			if (success) {
				SetStatusMessage("已替换选中项", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			} else {
				SetStatusMessage("替换失败", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			}
		} else {
			SetStatusMessage("请输入目标RID", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
		bReplaceRequested = false;
	}
	
	// Handle select all request
	if (bSelectAllRequested) {
		if (!searchResults.empty()) {
			SelectAllResults();
			SetStatusMessage("已全选", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		} else {
			SetStatusMessage("没有搜索结果可选择", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
		bSelectAllRequested = false;
	}
	
	// Handle deselect all request
	if (bDeselectAllRequested) {
		if (!searchResults.empty()) {
			DeselectAllResults();
			SetStatusMessage("已取消全选", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		} else {
			SetStatusMessage("没有搜索结果可取消选择", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
		bDeselectAllRequested = false;
	}
	
	// Handle replace all request
	if (bReplaceAllRequested) {
		if (searchResults.empty()) {
			SetStatusMessage("没有搜索结果可修改", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		} else if (strlen(targetId) > 0) {
			ReplaceAllResults(std::string(targetId));
		} else {
			SetStatusMessage("请输入目标RID", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
		bReplaceAllRequested = false;
	}
}

void PlayerChaser::SyncUI()
{
	// Sync from UI to atomic values
	bEnable.store(bEnableUI);
}

void PlayerChaser::SearchForValue(const std::string& valueStr)
{
	// Clear previous results
	searchResults.clear();
	firstScanResults.clear();
	
	try {
		// Convert string to uint32_t
		uint32_t searchValue = std::stoul(valueStr);
		
		// Define memory regions to search
		struct MemoryRegion {
			uintptr_t startAddress;
			DWORD size;
		};
		
		// List of memory regions to search (expanded search范围)
		std::vector<MemoryRegion> regions = {
			{ DMA::BaseAddress, 0x5000000 }, // Base region - 80MB
			{ DMA::BaseAddress + 0x10000000, 0x3000000 }, // Additional region - 48MB
			{ DMA::BaseAddress + 0x20000000, 0x2000000 } // Additional region - 32MB
		};
		
		// Total results found
		int totalResults = 0;
		
		// Search in each memory region
		for (const auto& region : regions) {
			// Allocate buffer for reading memory
			std::vector<uint32_t> buffer(static_cast<size_t>(region.size) / sizeof(uint32_t));
			DWORD bytesRead = 0;
			
			// Read memory from game
			if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, region.startAddress, 
				(BYTE*)buffer.data(), region.size, &bytesRead, VMMDLL_FLAG_NOCACHE)) {
				
				// Search for the value
				size_t count = bytesRead / sizeof(uint32_t);
				for (size_t i = 0; i < count; i++) {
					if (buffer[i] == searchValue) {
						SearchResult result;
						result.address = region.startAddress + (i * sizeof(uint32_t));
						result.value = buffer[i];
						result.previousValue = buffer[i]; // Store initial value
						result.changed = false; // Initially no change
						result.selected = false; // Initially not selected
						searchResults.push_back(result);
						firstScanResults.push_back(result); // Also store in first scan results
						totalResults++;
					}
				}
			}
		}
		
		if (totalResults == 0) {
			SetStatusMessage("未找到匹配的值", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		}
	} catch (const std::invalid_argument&) {
		// Handle invalid value error
		SetStatusMessage("无效的搜索值", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	} catch (const std::out_of_range&) {
		// Handle out of range error
		SetStatusMessage("搜索值超出范围", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	} catch (...) {
		// Handle other errors
		SetStatusMessage("搜索过程中发生错误", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	}
}

void PlayerChaser::NextScan(const std::string& valueStr)
{
	// If we have no first scan results, do nothing
	if (firstScanResults.empty()) {
		SetStatusMessage("请先执行首次搜索", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
		return;
	}
	
	// If valueStr is empty, we just want to update the values
	bool updateValuesOnly = valueStr.empty();
	uint32_t searchValue = 0;
	
	if (!updateValuesOnly) {
		try {
			searchValue = std::stoul(valueStr);
		} catch (const std::invalid_argument&) {
			// Handle invalid value error
			SetStatusMessage("无效的扫描值，请输入有效的32位整数", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			return;
		} catch (const std::out_of_range&) {
			// Handle out of range error
			SetStatusMessage("扫描值超出范围，必须是有效的32位整数", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			return;
		} catch (...) {
			// Handle other errors
			SetStatusMessage("扫描值格式错误", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			return;
		}
	}
	
	// Update the current search results with current memory values
	std::vector<SearchResult> newResults;
	int readErrorCount = 0;
	
	for (auto& result : firstScanResults) {
		uint32_t currentValue = 0;
		DWORD bytesRead = 0;
		
		// Read current value from memory
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, result.address, 
			(BYTE*)&currentValue, sizeof(uint32_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && 
			bytesRead == sizeof(uint32_t)) {
			
			// Check if value matches criteria
			bool matches = updateValuesOnly ? true : (currentValue == searchValue);
			
			if (matches) {
				SearchResult newResult;
				newResult.address = result.address;
				newResult.value = currentValue;
				newResult.previousValue = result.value; // Keep original value for comparison
				newResult.changed = (currentValue != result.previousValue); // Check if value changed
				newResult.selected = false; // Initially not selected
				newResults.push_back(newResult);
			}
		} else {
			readErrorCount++;
		}
	}
	
	// Update search results with narrowed down list
	searchResults = newResults;
	
	// Status message based on operation type
	if (updateValuesOnly) {
		if (readErrorCount > 0) {
			SetStatusMessage("值更新完成，无法读取 " + std::to_string(readErrorCount) + " 个地址", ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
		} else {
			SetStatusMessage("值更新完成", ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		}
	} else {
		std::string status = "下次扫描完成，找到 " + std::to_string(newResults.size()) + " 个匹配结果";
		if (readErrorCount > 0) {
			status += "，无法读取 " + std::to_string(readErrorCount) + " 个地址";
		}
		SetStatusMessage(status, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
	}
}

void PlayerChaser::UpdateSearchResults()
{
	// Update the values in searchResults with current memory values
	for (auto& result : searchResults) {
		uint32_t currentValue = 0;
		DWORD bytesRead = 0;
		
		// Read current value from memory
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, result.address, 
			(BYTE*)&currentValue, sizeof(uint32_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && 
			bytesRead == sizeof(uint32_t)) {
			result.previousValue = result.value; // Store previous value
			result.value = currentValue; // Update current value
			result.changed = (result.value != result.previousValue); // Check if value changed
		}
	}
}

bool PlayerChaser::ReplaceValue(uintptr_t address, const std::string& newValueStr)
{
	try {
		// Convert string to uint32_t
		uint32_t newValue = std::stoul(newValueStr);
		
		// Write the new value to memory
		DWORD bytesWritten = 0;
		bool success = VMMDLL_MemWrite(DMA::vmh, DMA::PID, address, 
			(BYTE*)&newValue, sizeof(uint32_t));
		
		return success;
	} catch (...) {
		// Handle conversion error
		return false;
	}
}

void PlayerChaser::SelectAllResults()
{
	for (auto& result : searchResults) {
		result.selected = true;
	}
}

void PlayerChaser::DeselectAllResults()
{
	for (auto& result : searchResults) {
		result.selected = false;
	}
}

void PlayerChaser::ReplaceAllResults(const std::string& newValueStr)
{
	try {
		// Convert string to uint32_t
		uint32_t newValue = std::stoul(newValueStr);
		
		// Count selected results
		int selectedCount = 0;
		for (const auto& result : searchResults) {
			if (result.selected) {
				selectedCount++;
			}
		}
		
		int totalResultsToProcess = (selectedCount > 0) ? selectedCount : (int)searchResults.size();
		int successCount = 0;
		int failCount = 0;
		
		// Status message during replacement
		SetStatusMessage("开始替换...", ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
		
		// Replace results
		for (auto& result : searchResults) {
			// Process only selected results if any are selected, otherwise process all
			if (selectedCount == 0 || result.selected) {
				bool success = ReplaceValue(result.address, newValueStr);
				if (success) {
					// Update the result value immediately
					result.value = newValue;
					result.previousValue = newValue;
					result.changed = false; // Value is now stable
					successCount++;
				} else {
					failCount++;
				}
			}
		}
		
		// Final status message
		std::stringstream ss;
		if (selectedCount > 0) {
			ss << "替换完成，成功替换 " << successCount << " 个选中结果，失败 " << failCount << " 个";
		} else {
			ss << "替换完成，成功替换 " << successCount << " 个结果，失败 " << failCount << " 个";
		}
		
		// Use different color based on success rate
		ImVec4 statusColor = (successCount > 0) ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		SetStatusMessage(ss.str(), statusColor);
		
	} catch (const std::invalid_argument&) {
		// Handle invalid value error
		SetStatusMessage("无效的替换值，请输入有效的32位整数", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	} catch (const std::out_of_range&) {
		// Handle out of range error
		SetStatusMessage("替换值超出范围，必须是有效的32位整数", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	} catch (...) {
		// Handle other errors
		SetStatusMessage("替换过程中发生错误", ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
	}
}

void PlayerChaser::SetStatusMessage(const std::string& message, const ImVec4& color)
{
	statusMessage = message;
	statusColor = color;
}