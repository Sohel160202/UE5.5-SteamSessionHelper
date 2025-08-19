// SteamSessionBP.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SteamSessionBP.generated.h"

/** Simple “done/ok” dispatcher */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamHelperSimple);
/** Error with human readable reason */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamHelperFailure, const FString&, Error);
/** Find results (plus optional error string; empty on success) */
USTRUCT(BlueprintType)
struct FSteamSearchResultLite
{
	GENERATED_BODY()

	/** Index into the most recent Find search array (used by Join) */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	int32 Index = INDEX_NONE;

	/** What you can show in UI (Owner — Map [Keyword]) */
	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	FString OwnerName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	FString MapName;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	FString KeywordTag;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	int32 OpenSlots = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Session")
	int32 PingMs = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamFindComplete, const TArray<FSteamSearchResultLite>&, Results, const FString&, ErrorText);

/** -------------------- Create -------------------- */
UCLASS()
class USteamCreateSessionAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnSteamHelperSimple  OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnSteamHelperFailure OnFailure;

	/** Creates a Steam lobby-style presence session. Call LoadLevel?(Map)?listen in BP after success. */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Steam|Session")
	static USteamCreateSessionAsync* CreateSteamSession(UObject* WorldContextObject, int32 PublicConnections = 4, int32 PrivateConnections = 0, FString KeywordTag = TEXT("YourGameTag"));

	virtual void Activate() override;

private:
	TWeakObjectPtr<UObject> WorldCtx;
	int32 Pub = 4;
	int32 Priv = 0;
	FString Keyword;

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	FDelegateHandle CreateHandle;
};

/** -------------------- Find -------------------- */
UCLASS()
class USteamFindSessionsAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnSteamFindComplete OnComplete;

	/** Queries Steam for lobbies; optional keyword filter must match host’s tag */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Steam|Session")
	static USteamFindSessionsAsync* FindSteamSessions(UObject* WorldContextObject, int32 MaxResults = 100, FString KeywordTag = TEXT("YourGameTag"));

	virtual void Activate() override;

private:
	TWeakObjectPtr<UObject> WorldCtx;
	int32 Max = 100;
	FString Keyword;

	TSharedPtr<FOnlineSessionSearch> Search;
	void HandleFindSessionsComplete(bool bWasSuccessful);
	FDelegateHandle FindHandle;
};

/** -------------------- Join -------------------- */
UCLASS()
class USteamJoinSessionAsync : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnSteamHelperSimple  OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnSteamHelperFailure OnFailure;

	/**
	 * Joins the selected search result (by Index).
	 * If bNormalizeFlags is true, normalizes presence/lobbies flags before joining (UE 5.5 Steam quirk).
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Steam|Session")
	static USteamJoinSessionAsync* JoinSteamSession(UObject* WorldContextObject, const FSteamSearchResultLite& Result, bool bNormalizeFlags = true);

	virtual void Activate() override;

	/** Set by Find so Join can access the raw results. */
	static TWeakPtr<FOnlineSessionSearch> LastSearchRef;

private:
	TWeakObjectPtr<UObject> WorldCtx;
	FSteamSearchResultLite Pick;
	bool bNormalize = true;

	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void TravelToJoinedSession();

	FDelegateHandle JoinHandle;
};
