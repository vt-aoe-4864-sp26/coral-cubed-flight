import pandas as pd

filename = "analog.csv"
R = 0.2

df = pd.read_csv(filename)

P = ((df["V1"] - df["V2"]).abs() / R) * df["Vsys"]
Pow = P.mean()

print(f"The power the com board consumes is {Pow * 1000:.3f} mW")