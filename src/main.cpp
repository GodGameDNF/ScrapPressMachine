#include <algorithm>  // std::shuffle
#include <chrono>     // std::chrono::system_clock
#include <cmath>      // for sin, cos
#include <cstdlib>    // for rand
#include <ctime>      // for time
#include <iomanip>    // std::hex, std::uppercase
#include <iostream>
#include <random>  // std::default_random_engine
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace RE;

PlayerCharacter* p = nullptr;
BSScript::IVirtualMachine* vm = nullptr;
TESDataHandler* DH = nullptr;

BGSKeyword* kModRecipeFilterScrap = nullptr;
BGSKeyword* UnscrappableObject = nullptr;

TESObjectREFR* scrapContRef = nullptr;

//BGSListForm* componentScrapList = nullptr;

//std::vector<BGSComponent*> scrapComponents;
std::vector<TESObjectMISC*> scrapMaterials; 

struct equipmentComponentData
{
	TESForm* equipment = nullptr;
	uint32_t formListFormID = 0;
	BGSConstructibleObject* cobj = nullptr;
};

struct OMODComponentData
{
	TESForm* omod = nullptr;
	BGSConstructibleObject* cobj = nullptr;
};

std::unordered_map<TESForm*, BGSConstructibleObject*> equipmentCobjMap;
std::unordered_map<TESForm*, BGSConstructibleObject*> OMODCobjMap;

struct FormOrInventoryObj
{
	TESForm* form{ nullptr };  // TESForm 포인터를 가리키는 포인터
	uint64_t second_arg{ 0 };  // unsigned 64비트 정수
};

bool AddItemVM(BSScript::IVirtualMachine* vm, uint32_t i, TESObjectREFR* target, FormOrInventoryObj obj, uint32_t count, bool b1)
{
	using func_t = decltype(&AddItemVM);
	REL::Relocation<func_t> func{ REL::ID(1212351) };
	return func(vm, i, target, obj, count, b1);
}

bool RemoveItemVM(BSScript::IVirtualMachine* vm, uint32_t i, TESObjectREFR* target, FormOrInventoryObj obj, uint32_t count, bool b1, TESObjectREFR* sender)
{
	using func_t = decltype(&RemoveItemVM);
	REL::Relocation<func_t> func{ REL::ID(492460) };
	return func(vm, i, target, obj, count, b1, sender);
}

void RemoveAllItems(BSScript::IVirtualMachine* vm, uint32_t i, TESObjectREFR* del, TESObjectREFR* send, bool bMove)
{
	using func_t = decltype(&RemoveAllItems);
	REL::Relocation<func_t> func{ REL::ID(534058) };
	return func(vm, i, del, send, bMove);
}

bool GetQuestObject(TESObjectREFR* t)
{
	using func_t = decltype(&GetQuestObject);
	REL::Relocation<func_t> func{ REL::ID(608378) };
	return func(t);
}

uint64_t GetCount(TESObjectREFR* send)
{
	using func_t = decltype(&GetCount);
	REL::Relocation<func_t> func{ REL::ID(50725) };
	return func(send);
}

TBO_InstanceData* GetInstanceData(ExtraDataList* data)
{
	using func_t = decltype(&GetInstanceData);
	REL::Relocation<func_t> func{ REL::ID(1345655) };
	return func(data);
}

bool HasKeywordVM(BSScript::IVirtualMachine* vm, uint32_t i, TESForm* target, BGSKeyword* k)
{
	using func_t = decltype(&HasKeywordVM);
	REL::Relocation<func_t> func{ REL::ID(1400368) };
	return func(vm, i, target, k);
}

void AddStackItem(BGSInventoryList* list, TESBoundObject* bound, const BGSInventoryItem::Stack* stack, uint32_t* oldItemCount, uint32_t* newItemCount)
{
	using func_t = decltype(&AddStackItem);
	REL::Relocation<func_t> func{ REL::ID(19103) };
	return func(list, bound, stack, oldItemCount, newItemCount);
}

bool RemoveStack(BGSInventoryItem* item, BGSInventoryItem::Stack* stack)
{
	using func_t = decltype(&RemoveStack);
	REL::Relocation<func_t> func{ REL::ID(1266839) };
	return func(item, stack);
}

// 스크랩 결과를 아이템을 추가할 배열에 저장하는 함수. 소수점을 처리해야되서 합산해야함
void updateOrAddInjectPairs(std::vector<std::pair<TESForm*, float>>& injectDatas, TESForm* material, float count)
{
	for (std::pair<TESForm*, float>& injectData : injectDatas) {
		TESForm* injectForm = injectData.first;
		if (injectForm == material) {
			injectData.second += count;
			return;
		}
	}
	injectDatas.push_back({ material, count });
}

// cobj제작에 필요한 아이템이 정상적인 값인지 확인하는 함수
bool IsValidRequiredItem(TESForm* requiredItem)
{
	if (!requiredItem) {
		return false;
	}

	// 이름이 아예 없는 항목은 보이지도 않고 처리도 불가능하기 때문에 false 반환
	TESFullName* refBaseName = (TESFullName*)requiredItem;
	if (!refBaseName || TESFullName::GetFullName(*requiredItem, false).empty()) {
		return false;
	}

	// 폼 타입 체크
	stl::enumeration<ENUM_FORM_ID, std::uint8_t> itemType = requiredItem->formType;
	if (itemType != ENUM_FORM_ID::kARMO &&
		itemType != ENUM_FORM_ID::kWEAP &&
		itemType != ENUM_FORM_ID::kAMMO &&
		itemType != ENUM_FORM_ID::kMISC &&
		itemType != ENUM_FORM_ID::kCMPO &&
		itemType != ENUM_FORM_ID::kALCH &&
		itemType != ENUM_FORM_ID::kBOOK &&
		itemType != ENUM_FORM_ID::kNOTE) {
		return false;
	}

	return true;
}

// 벡터배열을 해시맵으로 변경
void convertToHashMap(const std::vector<OMODComponentData>& equipmentComponentDatas)
{
	for (const auto& data : equipmentComponentDatas) {
		TESForm* equipment = data.omod;
		// equipment가 nullptr이 아니면 해시맵에 추가
		if (equipment != nullptr) {
			// 기존 키가 있을 경우 componentDatas를 덧붙이고, 없을 경우 새로 삽입
			OMODCobjMap[equipment] = data.cobj;
		}
	}
}

void saveOMODComponents()
{
	std::vector<OMODComponentData> OMODComponentDatas;

	auto& cobjArray = DH->GetFormArray<BGSConstructibleObject>();

	for (BGSConstructibleObject* cobj : cobjArray) {
		if (!cobj) {
			continue;  // 데이터를 못 가져오면 다음으로
		}

		TESForm* createdOMOD = cobj->createdItem;
		if (!createdOMOD || createdOMOD->formType != ENUM_FORM_ID::kOMOD) {
			continue;  // 제작 아이템이 OMOD가 아니라면 다음으로
		}

		BGSMod::Attachment::Mod* targetmod = (BGSMod::Attachment::Mod*)createdOMOD;

		if (targetmod->formFlags & (1 << 7)) {  // mod colleciton 인지 확인함
			continue;
		}

		bool isExistCreateForm = false;

		for (OMODComponentData& componentData : OMODComponentDatas) {
			TESForm*& saveOMOD = componentData.omod;

			// omod는 먼저 저장된걸 덮어쓸수 없음 bool 변수 입력하고 탈출
			if (saveOMOD == createdOMOD) {
				isExistCreateForm = true;
				break;
			}
		}

		// 이미 데이터가 있어서 처리를 했으므로 다음으로
		if (isExistCreateForm) {
			continue;
		}

		OMODComponentData injectData;

		// 모두 통과했다면 구조체에 나머지 데이터를 저장하고 배열에 삽입함.
		injectData.omod = createdOMOD;
		injectData.cobj = cobj;
		OMODComponentDatas.push_back(injectData);
	}

	convertToHashMap(OMODComponentDatas);  // 해시맵으로 변경
}

// 벡터배열을 해시맵으로 변경
void convertToHashMap(const std::vector<equipmentComponentData>& equipmentComponentDatas)
{
	for (const auto& data : equipmentComponentDatas) {
		TESForm* equipment = data.equipment;
		// equipment가 nullptr이 아니면 해시맵에 추가
		if (equipment != nullptr) {
			// 기존 키가 있을 경우 componentDatas를 덧붙이고, 없을 경우 새로 삽입
			equipmentCobjMap[equipment] = data.cobj;
		}
	}
}

void saveEquipmentComponents()
{
	std::vector<equipmentComponentData> equipmentComponentDatas;

	auto& cobjArray = DH->GetFormArray<BGSConstructibleObject>();

	std::vector<BGSConstructibleObject*> scrapableEquipmentCobjs;

	for (BGSConstructibleObject* cobj : cobjArray) {
		if (!cobj) {
			continue;  // 데이터를 못 가져오면 다음으로
		}

		BGSTypedKeywordValueArray<KeywordType::kRecipeFilter> recipeKeywords = cobj->filterKeywords;
		if (!recipeKeywords.HasKeyword(kModRecipeFilterScrap)) {
			continue;  // 무기나 방어구에 대한 바닐라 스크랩 키워드가 없으면 다음으로
		}

		TESForm* createdFormList = cobj->createdItem;
		if (createdFormList && createdFormList->formType != ENUM_FORM_ID::kFLST) {
			continue;  // 폼리스트에 들었을때만 장비에 대한 스크랩 재료가 적용됨 폼리스트가 아니면 다음으로
		}

		scrapableEquipmentCobjs.push_back(cobj);
	}

	// 제작물중 레시피 키워드가 ModRecipeFilterScrap이고 createitem이 폼리스트인것만 저장되어있음
	for (BGSConstructibleObject* cobj : scrapableEquipmentCobjs) {
		if (!cobj) {
			continue;  // 데이터를 못 가져오면 다음으로
		}

		BGSListForm* itemList = (BGSListForm*)cobj->createdItem;
		if (!itemList || itemList->arrayOfForms.empty()) {
			continue;  // 폼리스트가 없거나 내용이 비었으면 다음으로
		}

		for (TESForm* equipment : itemList->arrayOfForms) {
			if (!equipment) {
				continue;
			}

			if (equipment->formType != ENUM_FORM_ID::kARMO && equipment->formType != ENUM_FORM_ID::kWEAP) {
				continue;  // 폼리스트의 항목이 무기도 아니고 방어구도 아니면 다음으로
			}

			bool isExistCreateForm = false;

			for (equipmentComponentData& componentData : equipmentComponentDatas) {
				TESForm*& saveEquipment = componentData.equipment;

				// 저장중인 데이터 구조체에 무기나 방어구가 있을때 처리
				if (saveEquipment == equipment) {
					isExistCreateForm = true;

					uint32_t CurrentListFormID = itemList->formID;           // 현재 제작식의 폼리스트의 폼아이디
					if (CurrentListFormID < componentData.formListFormID) {  // 현재 폼리스트와 저장된 폼 리스트 아이디를 비교함
						continue;
					}

					componentData.formListFormID = CurrentListFormID;  // 폼리스트의 폼아이디를 저장해둠
					componentData.cobj = cobj;

					break;
				}
			}

			// 이미 데이터가 있어서 처리를 했으므로 다음으로
			if (isExistCreateForm) {
				continue;
			}

			equipmentComponentData injectData;

			injectData.equipment = equipment;
			injectData.formListFormID = itemList->formID;
			injectData.cobj = cobj;

			equipmentComponentDatas.push_back(injectData);
		}
	}

	convertToHashMap(equipmentComponentDatas);
}

// 인벤토리 리스트를 불러오고 없으면 만드는 함수
BGSInventoryList* checkAndCreateInvList(TESObjectREFR* container)
{
	BGSInventoryList* invList = container->inventoryList;  // 인벤토리 리스트 변수로 지정

	if (!invList) {
		TESObjectCONT* insertContainer = (TESObjectCONT*)(container->data.objectReference);
		if (!insertContainer) {
			return nullptr;
		}

		container->CreateInventoryList(insertContainer);  // 없으면 인벤토리 리스트를 다시 만듬
		invList = container->inventoryList;
		if (!invList) {
			return nullptr;
		}
	}

	return invList;
}

int startScrap(std::monostate, TESObjectREFR* scrapMaterialCont, TESObjectREFR* workshopCont, bool bScrapAllItem)
{
	//auto readConfigStart = std::chrono::high_resolution_clock::now();

	if (!scrapContRef || !scrapMaterialCont) {
		return 0;
	}

	BGSInventoryList* iList = checkAndCreateInvList(scrapContRef);
	if (!iList) {
		return 0;
	}

	int iScrapCount = 0;
	bool bNonScrap = false;
	bool bWeapScrap = false;
	bool bArmoScrap = false;

	BSTArray<BGSInventoryItem> itemArray = iList->data;

	for (BGSInventoryItem invItem : itemArray) {
		TESForm* form = invItem.object;
		uint32_t mergedItemCount = invItem.GetCount();
		FormOrInventoryObj invObj;
		invObj.form = form;

		bool bScraping = false;

		if (!form) {
			continue;
		}

		// 무기 방어구 잡동사니만 처리함 (나중에 보고 omod 추가)
		stl::enumeration<ENUM_FORM_ID, std::uint8_t> itemType = form->formType;
		if (itemType != ENUM_FORM_ID::kARMO &&
			itemType != ENUM_FORM_ID::kWEAP &&
			itemType != ENUM_FORM_ID::kMISC) {
			RemoveItemVM(vm, 0, scrapContRef, invObj, mergedItemCount, true, scrapMaterialCont);
			bNonScrap = true;
			continue;
		}

		// unscrapable 키워드 있는 템 해체하기 안켜면 기본템 정보에서 키워드 확인하고 다음으로
		if (!bScrapAllItem && HasKeywordVM(vm, 0, form, UnscrappableObject)) {
			RemoveItemVM(vm, 0, scrapContRef, invObj, mergedItemCount, true, scrapMaterialCont);
			bNonScrap = true;
			continue;
		}

		std::vector<std::pair<TESForm*, float>> injectDatas;  // 0.5가 여러개라면 합산해야되서 백터배열에 저장하고 마지막에 처리

		if (itemType == ENUM_FORM_ID::kARMO || itemType == ENUM_FORM_ID::kWEAP) {
			// 인벤토리 리스트의 스택데이터로 데이터를 검색함
			auto stack = invItem.stackData.get();

			while (stack) {
				if (stack->extra) {
					// extra 데이터를 통해 모드 정보를 탐색
					ExtraDataList* extraData = stack->extra.get();
					if (extraData) {
						// 인스턴스 데이터를 땡겨옴
						auto modData = extraData->GetByType(kObjectInstance);

						BGSObjectInstanceExtra* iData = (BGSObjectInstanceExtra*)modData;

						// 부착물 란이 공란이면 ctd가 나기 때문에 values 값이 0이 아닌지 꼭 확인 필요
						if (modData && iData && iData->values) {
							std::span<BGSMod::ObjectIndexData> indexData = iData->GetIndexData();

							for (const auto& objIndex : indexData) {
								std::uint32_t objectID = objIndex.objectID;  // 이게 폼ID
								TESForm* omod = TESForm::GetFormByID(objectID);
								if (!omod || omod->formFlags & (1 << 7)) {  // mod colleciton 인지 확인함
									continue;
								}

								BGSConstructibleObject* cobj = nullptr;
								auto it = OMODCobjMap.find(omod);  //해시맵에 저장해둔걸 검색

								if (it != OMODCobjMap.end()) {
									// 키가 존재하면 값(constructible object)을 가져옴
									cobj = it->second;

									auto& requiredItems = cobj->requiredItems;
									if (!requiredItems) {
										continue;  // 제작 재료가 없음 다음으로
									}

									for (auto& tuple : *requiredItems) {
										TESForm* requiredItem = tuple.first;  // 첫 번째 값, TESForm* 타입
										uint32_t count = tuple.second.i;      // 두 번째 값, uint32_t 타입

										if (count <= 0 || !IsValidRequiredItem(requiredItem)) {  // 개수가 0이거나 인벤토리에 넣을수 없는 항목이면 다음으로
											continue;
										}

										// 스크랩하면 기본적으로 절반만 줌. 0.5라도 합쳐서 1이 되면 1을 주기떄문에 배열로 계산. 스택의 중첩개수를 넣음
										float updateCount = (float)count * stack->count / 2;

										updateOrAddInjectPairs(injectDatas, requiredItem, updateCount);
									}
								}
							}
						}
					}
				}
				stack = stack->nextStack.get();
			}

			BGSConstructibleObject* cobj = nullptr;
			auto it = equipmentCobjMap.find(form);  //해시맵에 저장해둔걸 검색

			if (it != equipmentCobjMap.end()) {
				// 키가 존재하면 값(constructible object)을 가져옴
				cobj = it->second;

				auto& requiredItems = cobj->requiredItems;
				if (requiredItems) {  // 제작 재료가 있다면 삽입함
					for (auto& tuple : *requiredItems) {
						TESForm* requiredItem = tuple.first;  // 첫 번째 값, TESForm* 타입
						uint32_t count = tuple.second.i;      // 두 번째 값, uint32_t 타입

						if (count <= 0 || !IsValidRequiredItem(requiredItem)) {  // 개수가 0이거나 인벤토리에 넣을수 없는 항목이면 다음으로
							continue;
						}

						float updateCount = (float)count * (float)mergedItemCount / 2;  // 스크랩하면 기본적으로 절반만 줌. 0.5라도 합쳐서 1이 되면 1을 주기떄문에 배열로 계산

						updateOrAddInjectPairs(injectDatas, requiredItem, updateCount);
					}
				}
			}

			for (std::pair<TESForm*, float> injectData : injectDatas) {
				int injectCount = injectData.second;
				TESForm* injectForm = injectData.first;
				FormOrInventoryObj tempObj;
				tempObj.form = injectForm;

				if (injectCount < 1) {
					continue;  // 재료의 소수점 합산이 1이 안되면 다음으로
				}

				if (!injectForm) {
					continue;
				}

				if (injectForm->formType == ENUM_FORM_ID::kCMPO) {
					BGSComponent* component = (BGSComponent*)injectForm;
					if (!component) {
						continue;
					}

					TESGlobal* scalar = component->modScrapScalar;
					if (!scalar) {
						continue;  // 실패 시 0 반환
					}

					TESObjectMISC* scrapMISC = component->scrapItem;
					if (!scrapMISC) {
						continue;  // 실패 시 0 반환
					}

					injectCount = injectCount * scalar->value;

					if (injectCount < 1) {
						continue;  // 스칼라를 곱해서 1이 안되면 다음으로
					}

					injectForm = scrapMISC;
				}

				tempObj.form = injectForm;

				if (workshopCont) {
					bool isScrapMaterial = false;

					for (TESForm* checkForm : scrapMaterials) {
						if (checkForm == injectForm) {
							AddItemVM(vm, 0, workshopCont, tempObj, injectCount, true);
							isScrapMaterial = true;
							break;
						}
					}

					if (!isScrapMaterial) {
						AddItemVM(vm, 0, scrapMaterialCont, tempObj, injectCount, true);  //아이템이 겹친 개수만큼 곱해서 넣음
					}
				} else {
					AddItemVM(vm, 0, scrapMaterialCont, tempObj, injectCount, true);  //아이템이 겹친 개수만큼 곱해서 넣음
				}

				bScraping = true;
			}
		} else {
			BSTArray<BSTTuple<TESForm*, BGSTypedFormValuePair::SharedVal>>* compoDatas = ((TESObjectMISC*)form)->componentData;
			if (compoDatas && !compoDatas->empty()) {
				for (int i = 0; i < compoDatas->size(); ++i) {
					auto& tuple = (*compoDatas)[i];

					BGSComponent* component = (BGSComponent*)tuple.first;
					uint32_t count = tuple.second.i;

					if (!component) {
						continue;
					}

					TESObjectMISC* scrapMISC = component->scrapItem;
					if (!scrapMISC) {
						continue;
					}

					FormOrInventoryObj tempObj;
					tempObj.form = scrapMISC;

					// 옵션을 켰으면 워크숍 상자가 null이 아님. 잡동사니를 워크숍으로
					if (workshopCont) {
						AddItemVM(vm, 0, workshopCont, tempObj, count * mergedItemCount, true);
					} else {
						AddItemVM(vm, 0, scrapMaterialCont, tempObj, count * mergedItemCount, true);
					}

					bScraping = true;
				}
			}
		}

		if (bScraping) {
			if (itemType == ENUM_FORM_ID::kARMO) {
				bArmoScrap = true;
			} else if (itemType == ENUM_FORM_ID::kWEAP) {
				bWeapScrap = true;
			}

			++iScrapCount;
			RemoveItemVM(vm, 0, scrapContRef, invObj, mergedItemCount, false, nullptr);  // 템을 넣었으니 원본은 삭제
		} else {
			RemoveItemVM(vm, 0, scrapContRef, invObj, mergedItemCount, true, scrapMaterialCont);  // 스크랩을 안했으니 창고로 바로 이동
			bNonScrap = true;
		}
	}

	/*
	int property ejectType_nonScrap = 0 autoreadonly
	int property ejectType_MISC = 1 autoreadonly
	int property ejectType_MISC02 = 2 autoreadonly
	int property ejectType_WEAP = 3 autoreadonly
	int property ejectType_WEAP02 = 4 autoreadonly
	int property ejectType_ARMO = 5 autoreadonly
	int property ejectType_ARMO02 = 6 autoreadonly
	int property ejectType_ARWP = 7 autoreadonly
	int property ejectType_ARWP02 = 8 autoreadonly
	*/

	int enumScrapType = 0;

	if (bNonScrap) {
		enumScrapType = 10;
	}

	if (iScrapCount > 10) {
		if (bWeapScrap && bArmoScrap) {
			enumScrapType += 8;
		} else if (bWeapScrap) {
			enumScrapType += 4;
		} else if (bArmoScrap) {
			enumScrapType += 6;
		} else {
			enumScrapType += 2;
		}
	} else if (iScrapCount > 0) {
		if (bWeapScrap && bArmoScrap) {
			enumScrapType += 7;
		} else if (bWeapScrap) {
			enumScrapType += 3;
		} else if (bArmoScrap) {
			enumScrapType += 5;
		} else {
			enumScrapType += 1;
		}
	}

	//auto readConfigEnd = std::chrono::high_resolution_clock::now();
	//std::chrono::duration<double> readConfigDuration = readConfigEnd - readConfigStart;

	//logger::info("ReadConfig execution time: {} seconds", readConfigDuration.count());

	return enumScrapType;
}

bool validAddressLibraryCheck(std::monostate) {
	return true;
}

void saveScrapMaterials()
{
	auto& componentForms = DH->GetFormArray<RE::BGSComponent>();

	for (RE::BGSComponent* component : componentForms) {
		if (component) {
			TESObjectMISC* scrapMISC = component->scrapItem;
			if (!scrapMISC) {
				continue;
			}
			//scrapComponents.push_back(component);
			scrapMaterials.push_back(scrapMISC);
		}
	}
}

void OnF4SEMessage(F4SE::MessagingInterface::Message* msg)
{
	switch (msg->type) {
	case F4SE::MessagingInterface::kGameDataReady:
		{
			DH = RE::TESDataHandler::GetSingleton();
			p = PlayerCharacter::GetSingleton();

			kModRecipeFilterScrap = (BGSKeyword*)DH->LookupForm(0x01CF58B, "fallout4.esm");
			UnscrappableObject = (BGSKeyword*)DH->LookupForm(0x1CC46A, "fallout4.esm");
			scrapContRef = (TESObjectREFR*)DH->LookupForm(0x821, "SPM_ScrapPressMachine.esp");
			//componentScrapList = (BGSListForm*)DH->LookupForm(0x832, "SPM_ScrapPressMachine.esp");

			saveScrapMaterials();

			//gAmmoStock = (TESGlobal*)DH->LookupForm(0x0026, "ScrapGrinderAllItems.esp");

			//sellRoundsMult_1_5_List = (BGSListForm*)DH->LookupForm(0x002B, "ScrapGrinderAllItems.esp");
			//sellRoundsMult_0_5_List = (BGSListForm*)DH->LookupForm(0x002C, "ScrapGrinderAllItems.esp");
			//sellRoundsForcedList = (BGSListForm*)DH->LookupForm(0x002D, "ScrapGrinderAllItems.esp");
			//sellRoundsRemovedList = (BGSListForm*)DH->LookupForm(0x0034, "ScrapGrinderAllItems.esp");

			//saveEquipmentComponents();
			//saveOMODComponents();

			break;
		}
	case F4SE::MessagingInterface::kPostLoadGame:
		{
			static bool hasExecuted = false;  // static 변수로 한번만 실행되도록 설정
			if (!hasExecuted) {
				saveEquipmentComponents();
				saveOMODComponents();
				hasExecuted = true;  // 이후에는 실행되지 않도록 설정
			}
		}
	}
}

bool RegisterPapyrusFunctions(RE::BSScript::IVirtualMachine* a_vm)
{
	vm = a_vm;

	//REL::IDDatabase::Offset2ID o2i;
	//logger::info("0x0x80750: {}", o2i(0x80750));

	a_vm->BindNativeMethod("SPM_ScrapPress_F4SE"sv, "startScrap"sv, startScrap);
	a_vm->BindNativeMethod("SPM_ScrapPress_F4SE"sv, "validAddressLibraryCheck"sv, validAddressLibraryCheck);

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format("{}.log", Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("Global Log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::trace);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%^%l%$] %v"s);

	logger::info("{} v{}", Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor");
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}", ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	if (papyrus)
		papyrus->Register(RegisterPapyrusFunctions);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	if (message)
		message->RegisterListener(OnF4SEMessage);

	return true;
}
