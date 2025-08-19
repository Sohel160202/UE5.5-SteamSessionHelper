// SteamSessionBP.cpp
#include "SteamSessionBP.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h" // NAME_GameSession
#include "Interfaces/OnlineSessionInterface.h"
#include "Misc/EngineVersion.h"        // FEngineVersion::Current()

TWeakPtr<FOnlineSessionSearch> USteamJoinSessionAsync::LastSearchRef;

/* ---------- Small local helpers ---------- */
namespace SteamBP
{
	static inline UWorld* GetWorldFrom(UObject* Ctx)
	{
		if (!Ctx) return nullptr;
		if (UWorld* W = Ctx->GetWorld()) return W;
		if (const UObject* Outer = Ctx->GetOuter()) return Outer->GetWorld();
		return GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
	}

	static inline IOnlineSessionPtr GetSessions()
	{
		if (IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
		{
			return OSS->GetSessionInterface();
		}
		return nullptr;
	}

	// Use the engine changelist as BuildUniqueId so host & client match when built from the same engine build.
	static inline int32 GetBuildId()
	{
		return static_cast<int32>(FEngineVersion::Current().GetChangelist());
	}

	static inline void ApplyHostDefaults(FOnlineSessionSettings& S, int32 Pub, int32 Priv, const FString& Keyword)
	{
		S.NumPublicConnections = Pub;
		S.NumPrivateConnections = Priv;

		S.bIsLANMatch = false;
		S.bShouldAdvertise = true;
		S.bAllowJoinInProgress = true;
		S.bAllowInvites = false;
		S.bUsesPresence = true;
		S.bAllowJoinViaPresence = true;
		S.bAllowJoinViaPresenceFriendsOnly = false;
		S.bUseLobbiesIfAvailable = true;
		S.bIsDedicated = false;

		S.BuildUniqueId = GetBuildId();

		// Store a simple tag so your Find can filter.
		S.Settings.Add(FName(TEXT("SEARCHKEYWORDS")),
			FOnlineSessionSetting(Keyword, EOnlineDataAdvertisementType::ViaOnlineService));
	}

	static inline FString MakeDisplayName(const FOnlineSessionSearchResult& R, const FString& Keyword, FString& OutOwner, FString& OutMap)
	{
		const FOnlineSession& Sess = R.Session;
		const auto& Sett = Sess.SessionSettings;

		OutOwner.Empty();
		OutMap.Empty();

		Sett.Get(FName(TEXT("MAPNAME")), OutMap);
		Sett.Get(FName(TEXT("OWNERNAME")), OutOwner);

		if (OutOwner.IsEmpty())
		{
			OutOwner = Sess.OwningUserName;
		}

		FString NameBits;
		if (!OutOwner.IsEmpty()) NameBits += OutOwner;
		if (!OutMap.IsEmpty())
		{
			if (!NameBits.IsEmpty()) NameBits += TEXT(" — ");
			NameBits += OutMap;
		}
		if (NameBits.IsEmpty()) NameBits = TEXT("Steam Lobby");

		// Append keyword if present (helps visualize tag)
		FString KW;
		if (Sett.Get(FName(TEXT("SEARCHKEYWORDS")), KW) && !KW.IsEmpty())
		{
			NameBits += FString::Printf(TEXT("  [%s]"), *KW);
		}
		else if (!Keyword.IsEmpty())
		{
			NameBits += FString::Printf(TEXT("  [%s]"), *Keyword);
		}
		return NameBits;
	}

	static inline FString JoinResultToText(EOnJoinSessionCompleteResult::Type R)
	{
		using T = EOnJoinSessionCompleteResult::Type;
		switch (R)
		{
		case T::Success:                 return TEXT("Success");
		case T::SessionIsFull:           return TEXT("Session is full");
		case T::SessionDoesNotExist:     return TEXT("Session does not exist");
		case T::CouldNotRetrieveAddress: return TEXT("Could not retrieve address");
		case T::AlreadyInSession:        return TEXT("Already in a session");
		case T::UnknownError:            return TEXT("Unknown error");
		default:                         return TEXT("Join failed");
		}
	}
}

/* -------------------- Create -------------------- */
USteamCreateSessionAsync* USteamCreateSessionAsync::CreateSteamSession(UObject* WorldContextObject, int32 PublicConnections, int32 PrivateConnections, FString KeywordTag)
{
	USteamCreateSessionAsync* Node = NewObject<USteamCreateSessionAsync>();
	Node->WorldCtx = WorldContextObject;
	Node->Pub = PublicConnections;
	Node->Priv = PrivateConnections;
	Node->Keyword = KeywordTag;
	return Node;
}

void USteamCreateSessionAsync::Activate()
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (!Sessions.IsValid())
	{
		OnFailure.Broadcast(TEXT("Online Subsystem not available"));
		return;
	}

	const FName Name = NAME_GameSession;
	if (Sessions->GetNamedSession(Name))
	{
		OnFailure.Broadcast(TEXT("Session already exists"));
		return;
	}

	FOnlineSessionSettings Settings;
	SteamBP::ApplyHostDefaults(Settings, Pub, Priv, Keyword);

	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(
			this, &USteamCreateSessionAsync::HandleCreateSessionComplete));

	const bool bOk = Sessions->CreateSession(0, Name, Settings);
	if (!bOk)
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		OnFailure.Broadcast(TEXT("CreateSession() returned false"));
	}
}

void USteamCreateSessionAsync::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}

	if (!bWasSuccessful)
	{
		OnFailure.Broadcast(TEXT("CreateSession failed"));
		return;
	}

	OnSuccess.Broadcast();
}

/* -------------------- Find -------------------- */
USteamFindSessionsAsync* USteamFindSessionsAsync::FindSteamSessions(UObject* WorldContextObject, int32 MaxResults, FString KeywordTag)
{
	USteamFindSessionsAsync* Node = NewObject<USteamFindSessionsAsync>();
	Node->WorldCtx = WorldContextObject;
	Node->Max = MaxResults;
	Node->Keyword = KeywordTag;
	return Node;
}

void USteamFindSessionsAsync::Activate()
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (!Sessions.IsValid())
	{
		OnComplete.Broadcast({}, TEXT("Online Subsystem not available"));
		return;
	}

	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = Max;
	Search->bIsLanQuery = false;

	// Ask Steam for lobbies; presence helps with lobby results in OSS
	Search->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
		FOnFindSessionsCompleteDelegate::CreateUObject(
			this, &USteamFindSessionsAsync::HandleFindSessionsComplete));

	const bool bOk = Sessions->FindSessions(0, Search.ToSharedRef());
	if (!bOk)
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		OnComplete.Broadcast({}, TEXT("FindSessions() returned false"));
	}
}

void USteamFindSessionsAsync::HandleFindSessionsComplete(bool bWasSuccessful)
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	TArray<FSteamSearchResultLite> Out;

	if (!bWasSuccessful || !Search.IsValid())
	{
		OnComplete.Broadcast(Out, TEXT("Find failed"));
		return;
	}

	Out.Reserve(Search->SearchResults.Num());

	int32 i = 0;
	for (const FOnlineSessionSearchResult& R : Search->SearchResults)
	{
		const FOnlineSessionSettings& S = R.Session.SessionSettings;

		// Filter by keyword if host provided one
		FString KW;
		S.Get(FName(TEXT("SEARCHKEYWORDS")), KW);
		if (!Keyword.IsEmpty())
		{
			if (!KW.Equals(Keyword, ESearchCase::IgnoreCase))
			{
				++i;
				continue;
			}
		}

		FSteamSearchResultLite Row;
		Row.Index = i;

		FString Owner, Map;
		Row.DisplayName = SteamBP::MakeDisplayName(R, Keyword, Owner, Map);
		Row.OwnerName = Owner;
		Row.MapName = Map;
		Row.KeywordTag = KW.IsEmpty() ? Keyword : KW;

		Row.MaxPlayers = R.Session.SessionSettings.NumPublicConnections + R.Session.SessionSettings.NumPrivateConnections;
		Row.OpenSlots = R.Session.NumOpenPublicConnections;
		Row.PingMs = R.PingInMs;

		Out.Add(Row);
		++i;
	}

	USteamJoinSessionAsync::LastSearchRef = Search;

	OnComplete.Broadcast(Out, Out.Num() > 0 ? TEXT("") : TEXT("No servers found"));
	Search.Reset(); // keep weak-ref alive via LastSearchRef
}

/* -------------------- Join -------------------- */
USteamJoinSessionAsync* USteamJoinSessionAsync::JoinSteamSession(UObject* WorldContextObject, const FSteamSearchResultLite& Result, bool bNormalizeFlags)
{
	USteamJoinSessionAsync* Node = NewObject<USteamJoinSessionAsync>();
	Node->WorldCtx = WorldContextObject;
	Node->Pick = Result;
	Node->bNormalize = bNormalizeFlags;
	return Node;
}

void USteamJoinSessionAsync::Activate()
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (!Sessions.IsValid())
	{
		OnFailure.Broadcast(TEXT("Online Subsystem not available"));
		return;
	}

	TSharedPtr<FOnlineSessionSearch> Search = LastSearchRef.Pin();
	if (!Search.IsValid() || !Search->SearchResults.IsValidIndex(Pick.Index))
	{
		OnFailure.Broadcast(TEXT("Stale or invalid search result"));
		return;
	}

	// Optionally normalize flags before join (UE 5.5 Steam equivalence)
	if (bNormalize)
	{
		FOnlineSessionSearchResult& Mutable = Search->SearchResults[Pick.Index];
		FOnlineSessionSettings& S = Mutable.Session.SessionSettings;
		const bool UseLobbyPresence = S.bUsesPresence || S.bUseLobbiesIfAvailable;
		S.bUsesPresence = UseLobbyPresence;
		S.bUseLobbiesIfAvailable = UseLobbyPresence;
	}

	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
		FOnJoinSessionCompleteDelegate::CreateUObject(
			this, &USteamJoinSessionAsync::HandleJoinSessionComplete));

	const FOnlineSessionSearchResult& ToJoin = Search->SearchResults[Pick.Index];
	const bool bOk = Sessions->JoinSession(0, NAME_GameSession, ToJoin);
	if (!bOk)
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		OnFailure.Broadcast(TEXT("JoinSession() returned false"));
	}
}

void USteamJoinSessionAsync::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (Sessions.IsValid())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		OnFailure.Broadcast(SteamBP::JoinResultToText(Result));
		return;
	}

	TravelToJoinedSession();
	OnSuccess.Broadcast();
}

void USteamJoinSessionAsync::TravelToJoinedSession()
{
	IOnlineSessionPtr Sessions = SteamBP::GetSessions();
	if (!Sessions.IsValid())
	{
		OnFailure.Broadcast(TEXT("No session interface during travel"));
		return;
	}

	FString ConnectString;
	if (!Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		OnFailure.Broadcast(TEXT("Could not resolve connect string"));
		return;
	}

	if (UWorld* W = SteamBP::GetWorldFrom(WorldCtx.Get()))
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0))
		{
			PC->ClientTravel(ConnectString, TRAVEL_Absolute);
			return;
		}
	}
	OnFailure.Broadcast(TEXT("Could not issue client travel"));
}
