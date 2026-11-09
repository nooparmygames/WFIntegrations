# WFIntegrations

WFIntegrations connects Wise Feline Utility AI to Smart Objects, the Gameplay Ability System,
and Wise Feline Influence Maps. It works with Lite and Ultimate.

The precompiled Windows packages for version 1.1.0 require Wise Feline 3.3.0 and Wise Feline
Influence Maps 3.2.1. These are the dependency versions used to build and validate the supplied
binaries. Older combinations have not been verified. Install Influence Maps even if you only
use the Smart Object or GAS classes: the integration module depends on all three systems.

Use a package built for your exact Unreal Engine version. Put the plugin in your project's
`Plugins/WFIntegrations` folder, enable it, and restart the editor. Unreal also enables the
required Smart Objects, Gameplay Abilities and Gameplay Behavior Smart Objects engine plugins.
Precompiled Windows packages work in Blueprint-only projects. Other target platforms require
compiling the source for that platform.

The [integrations guide](https://www.nooparmygames.com/WF-UtilityAI-Unreal/articles/integrations.html)
explains the individual actions and considerations.

## Move to an influence-map result

Add `WFMoveToInfluenceMapValueAction` to a behavior when an agent should move to a location
chosen from an influence map. For example, set `MapName` to `Danger`, `SearchMode` to
`LowestValue`, `SearchCondition` to `LessOrEqual`, and `SearchValue` to `0.2`. With
`SearchRadius` set to `10`, the action searches a square extending ten cells from the agent
in each direction. It chooses the lowest cell in that area and only proceeds if its value
is at most 0.2. Equal extrema use the map's closest-to-center tie breaking.

The action selects once when activated and keeps that destination until the move ends.
Changing map values during movement does not restart the request. You can use a Vector
blackboard key as the search center, and enable interest/self templates to modify the
values used by the search.

The chosen cell must project onto navigation without moving into a different cell, and the
controller must find a complete path. If the cell cannot be reached, the action finishes;
it does not keep trying neighboring cells. Give the behavior a fallback action for this case.
`bHasDestination` means projection succeeded, while `bMoveSucceeded` reports successful arrival.
Both reset at the next activation. `AcceptanceRadius` and `bIncludeAgentRadius` control how
close path following must get, so arrival can stop short of the exact cell center.

Influence values choose the destination. Movement uses ordinary NavMesh path costs, so a path
to a low-danger cell can still cross dangerous cells. A sample that promises safe routes needs
an additional routing strategy and must check the path itself.

## 1.1.0 - 2026-11-09

- Smart Object searches now use the action's `ClaimPriority`. Set the same priority on
  `WFSmartObjectConsideration` so availability scoring matches the action. Higher priority
  can replace a lower-priority reservation, but cannot take an occupied slot.
- A displaced Smart Object claim clears its original blackboard publication. A plain-use
  action stops its move when its reservation is released, leaving the new owner's claim intact.
- Added `WFMoveToInfluenceMapValueAction` for stable movement to a matching, highest-value or
  lowest-value influence-map destination, with optional interest and self templates.
- Blackboard readers find the blackboard on the AIController as well as the pawn. They refresh
  after possession changes and can find a component created after BeginPlay.
- Influence-map considerations return zero when the actor or a required blackboard is missing.
- Smart Object use tracks its own movement request and claim publication. Interrupting it cleans
  up that work without stopping a replacement move or clearing a replacement blackboard claim.
- Successful movement starts Smart Object use even when path following stops within the pawn's
  acceptance radius. A released handle left in the blackboard no longer prevents a new claim.
- GAS actions and considerations refresh the controlled actor's ability system. An ability ending
  on the previous pawn no longer finishes the current pawn's action.
- Cancelling a GameplayBehavior Smart Object action stops the behavior on the pawn that started
  it and releases the slot, including when that pawn has already been unpossessed.

Abilities started by `WFActivateAbilityAction` keep their own lifetime when the action is
interrupted. The action removes its completion listener; it does not cancel the ability.
