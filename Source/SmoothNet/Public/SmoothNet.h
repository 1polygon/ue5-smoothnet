#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SmoothNet.generated.h"

UENUM(BlueprintType)
enum class EAuthorityMode : uint8
{
	Owner,
	PawnOwner,
	Server
};

USTRUCT()
struct FSnapshot
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;
	UPROPERTY()
	FQuat Rotation;
	UPROPERTY()
	FVector Velocity;
	UPROPERTY()
	float Time;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SMOOTHNET_API USmoothNet : public UActorComponent
{
	GENERATED_BODY()

public:
	USmoothNet();
private:
	float LastTick;
	float Time = 0;

	FVector LastLocation;
	FVector Velocity;

	FSnapshot LastSnapshot;
	TArray<FSnapshot> Buffer = TArray<FSnapshot>();

	USceneComponent* Target;

	UFUNCTION(Server, Unreliable, WithValidation)
	void SendSnapshotToServer(const FSnapshot Snapshot);

	UFUNCTION(NetMulticast, Unreliable, WithValidation)
	void SendSnapshotToClients(const FSnapshot Snapshot);

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	bool UseActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	FString ComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	EAuthorityMode AuthorityMode = EAuthorityMode::Server;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	int TickRate = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Delay = 0.1;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "SmoothNet")
	bool HasAuthority() const;
	
	UFUNCTION(BlueprintCallable, Category = "SmoothNet")
	static FVector Hermite(float Alpha, FVector V1, FVector V2, FVector V3, FVector V4);

	UFUNCTION(BlueprintCallable, Category = "SmoothNet")
	static FRotator Slerp(FRotator R1, FRotator R2, float Alpha);
};