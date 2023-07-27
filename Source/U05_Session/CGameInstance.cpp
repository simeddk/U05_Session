#include "CGameInstance.h"
#include "Global.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "Blueprint/UserWidget.h"
#include "Widgets/CMenu.h"
#include "Widgets/CMenuBase.h"

const static FName SESSION_NAME = TEXT("MySession");

UCGameInstance::UCGameInstance(const FObjectInitializer& ObjectInitializer)
{
	CLog::Log("GameInstance::Constructor Called");

	CHelpers::GetClass(&MenuWidgetClass, "/Game/Widgets/WB_Menu");
	CHelpers::GetClass(&InGameWidgetClass, "/Game/Widgets/WB_InGame");
}

void UCGameInstance::Init()
{
	Super::Init();

	CLog::Log("GameInstance::Init Called");

	IOnlineSubsystem* oss = IOnlineSubsystem::Get();
	if (!!oss)
	{
		CLog::Log("OSS Name : " + oss->GetSubsystemName().ToString());

		//Session Event Binding
		SessionInterface = oss->GetSessionInterface();
		if (SessionInterface.IsValid())
		{
			SessionInterface->OnCreateSessionCompleteDelegates.AddUObject(this, &UCGameInstance::OnCreateSessionComplete);
			SessionInterface->OnDestroySessionCompleteDelegates.AddUObject(this, &UCGameInstance::OnDestroySessionComplete);
			SessionInterface->OnFindSessionsCompleteDelegates.AddUObject(this, &UCGameInstance::OnFindSessionComplete);
			SessionInterface->OnJoinSessionCompleteDelegates.AddUObject(this, &UCGameInstance::OnJoinSessionComplete);
		}
		
	}
	else
	{
		CLog::Log("OSS Not Found!");
	}

}

void UCGameInstance::LoadMenu()
{
	CheckNull(MenuWidgetClass);

	Menu = CreateWidget<UCMenu>(this, MenuWidgetClass);
	CheckNull(Menu);

	Menu->SetOwingGameInstance(this);
	Menu->Attach();
}

void UCGameInstance::LoadInGameMenu()
{
	CheckNull(InGameWidgetClass);

	UCMenuBase* inGameWidget = CreateWidget<UCMenuBase>(this, InGameWidgetClass);
	CheckNull(inGameWidget);

	inGameWidget->SetOwingGameInstance(this);
	inGameWidget->Attach();
}

void UCGameInstance::Host()
{
	if (SessionInterface.IsValid())
	{
	 	auto session = SessionInterface->GetNamedSession(SESSION_NAME);
		if (!!session)
		{
			SessionInterface->DestroySession(SESSION_NAME);
		}
		else
		{
			CreateSession();
		}
	}
}

void UCGameInstance::CreateSession()
{
	if (SessionInterface.IsValid())
	{
		//Todo. OSS에 따라서 세션 생성 옵션 세팅을 바꿀거다.
		FOnlineSessionSettings sessionSettings;
		sessionSettings.bIsLANMatch = false;
		sessionSettings.NumPublicConnections = 5;
		sessionSettings.bShouldAdvertise = true;
		sessionSettings.bUsesPresence = true;

		SessionInterface->CreateSession(0, SESSION_NAME, sessionSettings);
	}
}

void UCGameInstance::Join(uint32 InSessionIndex)
{
	CheckFalse(SessionInterface.IsValid());
	CheckFalse(SessionSearch.IsValid());

	if (!!Menu)
		Menu->Detach();

	SessionInterface->JoinSession(0, SESSION_NAME, SessionSearch->SearchResults[InSessionIndex]);
}

void UCGameInstance::ReturnToMainMenu()
{
	APlayerController* controller = GetFirstLocalPlayerController();
	CheckNull(controller);
	controller->ClientTravel("/Game/Maps/MainMenu", ETravelType::TRAVEL_Absolute);
}

void UCGameInstance::FindSession()
{
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	if (SessionSearch.IsValid())
	{
		CLog::Log("Starting Find Session");

		//SessionSearch->bIsLanQuery = true;
		SessionSearch->MaxSearchResults = 100;
		SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
		SessionInterface->FindSessions(0, SessionSearch.ToSharedRef());
	}
}

void UCGameInstance::OnCreateSessionComplete(FName InSessionName, bool InSuccess)
{
	//세션 생성 실패
	if (InSuccess == false)
	{
		CLog::Log("Could not create Session!!");
		return;
	}

	//세션 생성 성공
	CLog::Log("Session Name : " + InSessionName.ToString());

	if (!!Menu)
		Menu->Detach();

	CLog::Print("Host");

	UWorld* world = GetWorld();
	CheckNull(world);
	world->ServerTravel("/Game/Maps/Play?listen");
}

void UCGameInstance::OnDestroySessionComplete(FName InSessionName, bool InSuccess)
{
	if (InSuccess == true)
		CreateSession();
}

void UCGameInstance::OnFindSessionComplete(bool InSuccess)
{
	if (InSuccess == true && 
		Menu != nullptr &&
		SessionSearch.IsValid())
	{
		TArray<FString> foundSession;
		
		CLog::Log("Finished Find Sessoin");

		CLog::Log("==========<Find Session Results>==========");
		for (const auto& searchResult : SessionSearch->SearchResults)
		{
			CLog::Log(" -> Session ID : " + searchResult.GetSessionIdStr());
			CLog::Log(" -> Ping : " + FString::FromInt(searchResult.PingInMs));

			foundSession.Add(searchResult.GetSessionIdStr());
		}
		CLog::Log("===========================================");


		Menu->SetSessionList(foundSession);
	}
}

void UCGameInstance::OnJoinSessionComplete(FName InSessionName, EOnJoinSessionCompleteResult::Type InResult)
{
	FString address;

	//조인 실패 시
	if (SessionInterface->GetResolvedConnectString(InSessionName, address) == false)
	{
		switch (InResult)
		{
			case EOnJoinSessionCompleteResult::SessionIsFull:			CLog::Log("SessionIsFull");				break;
			case EOnJoinSessionCompleteResult::SessionDoesNotExist:		CLog::Log("SessionDoesNotExist");		break;
			case EOnJoinSessionCompleteResult::CouldNotRetrieveAddress: CLog::Log("CouldNotRetrieveAddress");	break;
			case EOnJoinSessionCompleteResult::AlreadyInSession:		CLog::Log("AlreadyInSession");			break;
			case EOnJoinSessionCompleteResult::UnknownError:			CLog::Log("UnknownError");				break;
		}
		return;
	}

	//조인 성공 시
	CLog::Print("Join to " + address);

	APlayerController* controller = GetFirstLocalPlayerController();
	CheckNull(controller);
	controller->ClientTravel(address, ETravelType::TRAVEL_Absolute);
}

