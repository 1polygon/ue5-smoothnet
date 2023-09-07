#include "SmoothNet.h"
#include "DrawDebugHelpers.h"

USmoothNet::USmoothNet()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USmoothNet::BeginPlay()
{
	Super::BeginPlay();

	if (UseActor)
	{
		TargetComponent = GetOwner()->GetRootComponent();
	}
	else
	{
		TArray<USceneComponent*> Components;

		GetOwner()->GetComponents<USceneComponent>(Components);
		for (int i = 0; i < Components.Num(); i++)
		{
			const USceneComponent* Component = Components[i];
			if (Component->GetName().Equals(ComponentName, ESearchCase::IgnoreCase))
			{
				TargetComponent = const_cast<USceneComponent*>(Component);
				LastLocation = TargetComponent->GetComponentLocation();
				break;
			}
		}
	}

	UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(TargetComponent);
	if (IsValid(Component))
	{
		if (!HasAuthority())
		{
			Component->SetSimulatePhysics(false);
			// Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);				
		}
	}

	SetIsReplicated(true);
}


void USmoothNet::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(TargetComponent))
	{
		return;
	}

	const bool IsServer = IsNetMode(NM_DedicatedServer) || IsNetMode(NM_ListenServer);
	const float TimeSeconds = GetWorld()->TimeSeconds;

	if (HasAuthority())
	{
		if (TimeSeconds - LastTick > 1.0 / (float)TickRate)
		{
			FSnapshot Snapshot = FSnapshot();
			Snapshot.Location = TargetComponent->GetComponentLocation();
			Snapshot.Rotation = TargetComponent->GetComponentRotation().Quaternion();
			Snapshot.Velocity = Velocity;

			if (IsServer)
			{
				this->SendSnapshotToClients(Snapshot);
			}
			else
			{
				this->SendSnapshotToServer(Snapshot);
			}

			LastTick = TimeSeconds;
		}
	}
	else if (!IsServer)
	{
		while (Buffer.Num() > 0 && Time - Delay > Buffer[0].Time)
		{
			LastSnapshot = Buffer[0];
			Buffer.RemoveAt(0);
		}

		if (Buffer.Num() > 0 && Buffer[0].Time > 0)
		{
			const float Delta = Buffer[0].Time - LastSnapshot.Time;
			const float Alpha = (Time - Delay - LastSnapshot.Time) / Delta;
			const FVector Location = Hermite(Alpha, LastSnapshot.Location, Buffer[0].Location, LastSnapshot.Velocity,
			                                 Buffer[0].Velocity);
			const FQuat Rotation = FQuat::Slerp(LastSnapshot.Rotation, Buffer[0].Rotation, Alpha);
			TargetComponent->SetWorldLocationAndRotation(Location, Rotation);
			// DrawDebugBox(GetWorld(), Location, FVector(300, 200, 200), Rotation, FColor::Red, false, 0.1, 0, 1);
		}
	}

	Time += DeltaTime;
	Velocity = TargetComponent->GetComponentLocation() - LastLocation;
	LastLocation = TargetComponent->GetComponentLocation();
}

void USmoothNet::SendSnapshotToServer_Implementation(FSnapshot Snapshot)
{
	TargetComponent->SetWorldLocationAndRotation(Snapshot.Location, Snapshot.Rotation);
	this->SendSnapshotToClients(Snapshot);
}

bool USmoothNet::SendSnapshotToServer_Validate(FSnapshot Snapshot)
{
	return true;
}

void USmoothNet::SendSnapshotToClients_Implementation(FSnapshot Snapshot)
{
	if (!HasAuthority())
	{
		const bool IsServer = IsNetMode(NM_DedicatedServer) || IsNetMode(NM_ListenServer);
		if (!IsServer)
		{
			Snapshot.Time = Time;
			Buffer.Add(Snapshot);
		}
		// DrawDebugBox(GetWorld(), Snapshot.Location, FVector(150, 150, 150), Snapshot.Rotation, FColor::Yellow, false, 0.1, 0, 1);
	}
}

bool USmoothNet::SendSnapshotToClients_Validate(FSnapshot Snapshot)
{
	return true;
}

bool USmoothNet::HasAuthority() const
{
	const bool IsServer = IsNetMode(NM_DedicatedServer) || IsNetMode(NM_ListenServer);
	switch (AuthorityMode)
	{
	case EAuthorityMode::Client:
		return GetOwner()->GetInstigator()->IsLocallyControlled() && !IsServer;
	case EAuthorityMode::Server:
		return IsServer;
	default:
		return false;
	}
}

FVector USmoothNet::Hermite(const float Alpha, const FVector V1, const FVector V2, const FVector V3, const FVector V4)
{
	const float T2 = FMath::Pow(Alpha, 2);
	const float T3 = FMath::Pow(Alpha, 3);
	const float A = 1 - 3 * T2 + 2 * T3;
	const float B = T2 * (3 - 2 * Alpha);
	const float C = Alpha * FMath::Pow(Alpha - 1, 2);
	const float D = T2 * (Alpha - 1);

	return V1 * A + V2 * B + V3 * C + V4 * D;
}

FRotator USmoothNet::Slerp(const FRotator R1, const FRotator R2, const float Alpha)
{
	return FQuat::Slerp(R1.Quaternion(), R2.Quaternion(), Alpha).Rotator();
}
