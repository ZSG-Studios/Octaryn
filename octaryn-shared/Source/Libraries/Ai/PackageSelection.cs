namespace Octaryn.Shared.Ai;

public enum PackageEligibility { Ineligible, Eligible, Unknown }
public enum PackageSelectionStatus { Selected, NoMatch, Deferred, BudgetExceeded }
public interface IPackageEligibility
{
    PackageEligibility Query(int index,out string reason);
}
public readonly struct PackageSelectionResult(PackageSelectionStatus status,int index,int evaluated,string reason)
{
    public PackageSelectionStatus Status {get;}=status;
    public int Index {get;}=index;
    public int Evaluated {get;}=evaluated;
    public string Reason {get;}=reason;
}

// Ordered package admission. Unknown higher-priority behavior must not select a later package.
public static class PackageSelection
{
    public static PackageSelectionResult Evaluate(IPackageEligibility context,int count,int maximumEvaluations=256)
    {
        if(context is null || count<0 || count>256 || maximumEvaluations is <1 or >256)
            return new(PackageSelectionStatus.Deferred,-1,0,"Invalid package selection admission.");
        for(var index=0;index<count;++index)
        {
            if(index>=maximumEvaluations)return new(PackageSelectionStatus.BudgetExceeded,-1,index,"Package evaluation budget exhausted.");
            var state=context.Query(index,out var reason);
            if(state==PackageEligibility.Eligible)return new(PackageSelectionStatus.Selected,index,index+1,reason);
            if(state!=PackageEligibility.Ineligible)return new(PackageSelectionStatus.Deferred,index,index+1,reason);
        }
        return new(PackageSelectionStatus.NoMatch,-1,count,"No authored package matched.");
    }
}
