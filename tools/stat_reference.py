#!/usr/bin/env python3
"""Riferimenti numerici indipendenti per Statistics e Distribution (v1.18.0).

Calcola con scipy, numpy e statsmodels (fuori dal firmware) i valori attesi
di insiemi di dati fissi e scrive tests/fixtures/stat_reference.inc: i dati
(come vettori C++) e una tabella {chiave, valore}. tests/statistics_test.cpp
confronta il nucleo del firmware con questi valori e con le tolleranze
dichiarate in docs/reports/ANALISI_FONTI_v1.18.0.md (§6).

  python3 tools/stat_reference.py            # rigenera la fixture
  python3 tools/stat_reference.py --check    # confronta con la fixture salvata (cancello)

Le regole della fx-CG50 che scipy non ha (quartili Std e OnData, gruppi del
Med-Med) sono riscritte qui da zero, in modo indipendente dal codice C++.
"""
import math
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import scipy
import statsmodels
from scipy import optimize, stats
import statsmodels.api as sm
from statsmodels.formula.api import ols

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "tests" / "fixtures" / "stat_reference.inc"

# ---------------------------------------------------------------- dati
DATA = {
    "ds1_x": [1.2, 2.3, 2.3, 3.1, 4.8, 5.0, 5.5, 6.7, 7.1, 8.4, 9.9, 10.2],
    "ds1_f": [1, 2, 1, 3, 1, 1, 2, 1, 1, 1, 2, 1],
    "ds3_x": [3.4, 4.1, 2.9, 5.6, 6.2, 4.4, 3.8, 5.1, 4.9],
    "ds2_x": [float(i) for i in range(1, 13)],
    "ds2_y": [2.1, 3.9, 6.2, 7.8, 10.1, 12.2, 13.8, 16.1, 18.3, 19.9, 22.2, 24.1],
    "ds2_f": [1, 2, 1, 1, 3, 1, 2, 1, 1, 1, 1, 2],
    "ds4_y": [1.5, 1.9, 2.8, 4.1, 6.0, 9.2, 13.1, 20.4, 29.8, 45.1, 66.7, 99.0],
    "q_a": [7.0],
    "q_b": [3.0, 1.0],
    "q_c": [5.0, 2.0, 9.0],
    "q_d": [4.0, 1.0, 3.0, 2.0],
    "q_e": [6.0, 1.0, 5.0, 2.0, 4.0, 3.0, 8.0, 7.0],
    "q_e_f": [1, 2, 1, 3, 1, 1, 2, 1],
    "q_g": [2.0, 4.0, 4.0, 5.0, 7.0, 9.0, 9.0, 9.0, 10.0],
    "gof_o": [18.0, 22.0, 30.0, 30.0],
    "gof_e": [25.0, 25.0, 25.0, 25.0],
    "way2_o": [20.0, 30.0, 25.0, 30.0, 20.0, 25.0],
    "an1_a": [1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3],
    "an1_y": [12.1, 13.4, 11.8, 12.9, 14.2, 15.1, 13.9, 14.8, 15.4, 11.2, 10.8, 11.9],
    "an2_a": [1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2],
    "an2_b": [1, 1, 2, 2, 3, 3, 1, 1, 2, 2, 3, 3],
    "an2_y": [5.2, 5.8, 6.9, 7.3, 8.1, 7.7, 6.0, 6.6, 7.1, 7.9, 9.8, 10.4],
    "an3_a": [1, 1, 1, 2, 2, 2],
    "an3_b": [1, 2, 3, 1, 2, 3],
    "an3_y": [4.1, 5.3, 6.2, 4.9, 6.4, 7.8],
}
SIN_TRUE = (3.0, 0.8, 0.5, 2.0)
DATA["sin_x"] = [0.5 * i for i in range(40)]
DATA["sin_y"] = [SIN_TRUE[0] * math.sin(SIN_TRUE[1] * x + SIN_TRUE[2]) + SIN_TRUE[3] + 0.1 * math.sin(7.3 * i)
                 for i, x in enumerate(DATA["sin_x"])]
LOG_TRUE = (20.0, 0.7, 10.0)          # a, b, c di y = c/(1+a e^(-bx))
DATA["lgs_x"] = [float(i) for i in range(15)]
DATA["lgs_y"] = [LOG_TRUE[2] / (1 + LOG_TRUE[0] * math.exp(-LOG_TRUE[1] * x)) + 0.05 * math.cos(3.1 * i)
                 for i, x in enumerate(DATA["lgs_x"])]

REFS = []


def put(key, value):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError(f"{key}: valore non finito")
    REFS.append((key, value))


def arr(name):
    return np.array(DATA[name], dtype=float)


def expand(x, f):
    return np.repeat(np.asarray(x, dtype=float), np.asarray(f, dtype=int))


# ---------------------------------------------------------------- quartili della fx-CG50 (riscritti qui)
def med(v):
    v = sorted(v)
    n = len(v)
    return v[n // 2] if n % 2 else 0.5 * (v[n // 2 - 1] + v[n // 2])


def quart_std(v):
    v = sorted(v)
    n = len(v)
    if n == 1:
        return v[0], v[0]
    lo = v[: n // 2]
    hi = v[n // 2 + 1:] if n % 2 else v[n // 2:]
    return med(lo), med(hi)


def quart_ondata(v):
    v = sorted(v)
    n = len(v)
    if n == 1:
        return v[0], v[0]
    vals = sorted(set(v))
    cnt = [v.count(u) for u in vals]

    def q(num, den):
        cum = 0
        for i, u in enumerate(vals):
            cum += cnt[i]
            if cum * den == n * num:
                return 0.5 * (u + vals[i + 1]) if i + 1 < len(vals) else u
            if cum * den > n * num:
                return u
        return vals[-1]
    return q(1, 4), q(3, 4)


def one_var(prefix, x, f=None):
    d = expand(x, f) if f is not None else np.asarray(x, dtype=float)
    put(prefix + ".n", d.size)
    put(prefix + ".mean", d.mean())
    put(prefix + ".sum", d.sum())
    put(prefix + ".sumsq", (d * d).sum())
    put(prefix + ".popsd", d.std(ddof=0))
    if d.size > 1:
        put(prefix + ".sd", d.std(ddof=1))
    put(prefix + ".min", d.min())
    put(prefix + ".max", d.max())
    put(prefix + ".med", np.median(d))
    q1, q3 = quart_std(list(d))
    put(prefix + ".q1std", q1)
    put(prefix + ".q3std", q3)
    q1, q3 = quart_ondata(list(d))
    put(prefix + ".q1ondata", q1)
    put(prefix + ".q3ondata", q3)
    vals, counts = np.unique(d, return_counts=True)
    put(prefix + ".modcount", counts.max())
    put(prefix + ".modn", int((counts == counts.max()).sum()))
    put(prefix + ".mod", vals[np.argmax(counts)])


def two_var(prefix, x, y, f=None):
    w = np.ones(len(x)) if f is None else np.asarray(f, dtype=float)
    x = np.asarray(x, dtype=float)
    y = np.asarray(y, dtype=float)
    xe, ye = np.repeat(x, w.astype(int)), np.repeat(y, w.astype(int))
    put(prefix + ".n", xe.size)
    put(prefix + ".meanx", xe.mean()); put(prefix + ".meany", ye.mean())
    put(prefix + ".sumx", xe.sum()); put(prefix + ".sumy", ye.sum())
    put(prefix + ".sumx2", (xe * xe).sum()); put(prefix + ".sumy2", (ye * ye).sum()); put(prefix + ".sumxy", (xe * ye).sum())
    put(prefix + ".popsdx", xe.std()); put(prefix + ".popsdy", ye.std())
    put(prefix + ".sdx", xe.std(ddof=1)); put(prefix + ".sdy", ye.std(ddof=1))


# ---------------------------------------------------------------- regressioni
def reg_linear(prefix, x, y, f=None, swap=False):
    xe = expand(x, f) if f is not None else np.asarray(x, float)
    ye = expand(y, f) if f is not None else np.asarray(y, float)
    lr = stats.linregress(xe, ye)
    a, b = (lr.intercept, lr.slope) if swap else (lr.slope, lr.intercept)
    put(prefix + ".a", a); put(prefix + ".b", b)
    put(prefix + ".r", lr.rvalue); put(prefix + ".r2", lr.rvalue ** 2)
    res = ye - (lr.slope * xe + lr.intercept)
    put(prefix + ".mse", (res ** 2).sum() / (xe.size - 2))


def reg_poly(prefix, x, y, deg, f=None):
    xe = expand(x, f) if f is not None else np.asarray(x, float)
    ye = expand(y, f) if f is not None else np.asarray(y, float)
    c = np.polyfit(xe, ye, deg)
    for name, v in zip("abcde", c):
        put(prefix + "." + name, v)
    res = ye - np.polyval(c, xe)
    ss, st = (res ** 2).sum(), ((ye - ye.mean()) ** 2).sum()
    put(prefix + ".r2", 1 - ss / st)
    put(prefix + ".mse", ss / (xe.size - deg - 1))


def reg_transformed(prefix, x, y, kind, f=None):
    xe = expand(x, f) if f is not None else np.asarray(x, float)
    ye = expand(y, f) if f is not None else np.asarray(y, float)
    u = np.log(xe) if kind in ("log", "power") else xe
    v = ye if kind == "log" else np.log(ye)
    lr = stats.linregress(u, v)
    if kind == "log":
        a, b = lr.intercept, lr.slope
    elif kind == "abexp":
        a, b = math.exp(lr.intercept), math.exp(lr.slope)
    else:
        a, b = math.exp(lr.intercept), lr.slope
    put(prefix + ".a", a); put(prefix + ".b", b)
    put(prefix + ".r", lr.rvalue); put(prefix + ".r2", lr.rvalue ** 2)
    res = v - (lr.intercept + lr.slope * u)
    put(prefix + ".mse", (res ** 2).sum() / (xe.size - 2))


def medmed(prefix, x, y, f=None):
    xe = expand(x, f) if f is not None else np.asarray(x, float)
    ye = expand(y, f) if f is not None else np.asarray(y, float)
    order = np.lexsort((ye, xe))
    xe, ye = xe[order], ye[order]
    n = xe.size
    k, rem = divmod(n, 3)
    g1 = k + (1 if rem == 2 else 0)
    g2 = n - 2 * g1
    groups = [(0, g1), (g1, g1 + g2), (n - g1, n)]
    pts = [(np.median(xe[b:e]), np.median(ye[b:e])) for b, e in groups]
    a = (pts[2][1] - pts[0][1]) / (pts[2][0] - pts[0][0])
    b = sum(py - a * px for px, py in pts) / 3
    put(prefix + ".a", a); put(prefix + ".b", b)
    for i, (px, py) in enumerate(pts, start=1):
        put(f"{prefix}.x{i}", px); put(f"{prefix}.y{i}", py)


def norm_sin(p):
    a, b, c, d = p
    if b < 0:
        b, c = -b, math.pi - c
    if a < 0:
        a, c = -a, c + math.pi
    c = math.fmod(c, 2 * math.pi)
    if c <= -math.pi:
        c += 2 * math.pi
    if c > math.pi:
        c -= 2 * math.pi
    return a, b, c, d


def reg_sin(prefix):
    x, y = arr("sin_x"), arr("sin_y")
    fit = optimize.least_squares(lambda p: p[0] * np.sin(p[1] * x + p[2]) + p[3] - y, SIN_TRUE, method="lm",
                                 xtol=1e-15, ftol=1e-15, gtol=1e-15)
    a, b, c, d = norm_sin(fit.x)
    for name, v in zip("abcd", (a, b, c, d)):
        put(prefix + "." + name, v)
    ss = ((a * np.sin(b * x + c) + d - y) ** 2).sum()
    put(prefix + ".ss", ss)
    put(prefix + ".mse", ss / (x.size - 2))


def reg_logistic(prefix):
    x, y = arr("lgs_x"), arr("lgs_y")
    fit = optimize.least_squares(lambda p: p[2] / (1 + p[0] * np.exp(-p[1] * x)) - y, LOG_TRUE, method="lm",
                                 xtol=1e-15, ftol=1e-15, gtol=1e-15)
    a, b, c = fit.x
    for name, v in zip("abc", (a, b, c)):
        put(prefix + "." + name, v)
    ss = ((c / (1 + a * np.exp(-b * x)) - y) ** 2).sum()
    put(prefix + ".ss", ss)
    put(prefix + ".mse", ss / (x.size - 2))


# ---------------------------------------------------------------- test e intervalli
ALT = {0: "two-sided", -1: "less", 1: "greater"}


def ptail(cdf, sf, tail):
    return cdf if tail < 0 else sf if tail > 0 else min(1.0, 2 * min(cdf, sf))


def tests():
    d1, d3 = arr("ds1_x"), arr("ds3_x")
    for tail in (0, -1, 1):
        z = (d1.mean() - 5) / (2.5 / math.sqrt(d1.size))
        put(f"test.z1s.{tail}.z", z); put(f"test.z1s.{tail}.p", ptail(stats.norm.cdf(z), stats.norm.sf(z), tail))
        z = (d1.mean() - d3.mean()) / math.sqrt(2.5 ** 2 / d1.size + 1.2 ** 2 / d3.size)
        put(f"test.z2s.{tail}.z", z); put(f"test.z2s.{tail}.p", ptail(stats.norm.cdf(z), stats.norm.sf(z), tail))
        tt = stats.ttest_1samp(d1, 5, alternative=ALT[tail])
        put(f"test.t1s.{tail}.t", tt.statistic); put(f"test.t1s.{tail}.p", tt.pvalue)
        for pooled in (True, False):
            tt = stats.ttest_ind(d1, d3, equal_var=pooled, alternative=ALT[tail])
            put(f"test.t2s.{int(pooled)}.{tail}.t", tt.statistic); put(f"test.t2s.{int(pooled)}.{tail}.p", tt.pvalue)
            put(f"test.t2s.{int(pooled)}.{tail}.df", tt.df)
        ph = 55 / 100
        z = (ph - 0.5) / math.sqrt(0.5 * 0.5 / 100)
        put(f"test.z1p.{tail}.z", z); put(f"test.z1p.{tail}.p", ptail(stats.norm.cdf(z), stats.norm.sf(z), tail))
        p1, p2, pp = 45 / 100, 60 / 120, 105 / 220
        z = (p1 - p2) / math.sqrt(pp * (1 - pp) * (1 / 100 + 1 / 120))
        put(f"test.z2p.{tail}.z", z); put(f"test.z2p.{tail}.p", ptail(stats.norm.cdf(z), stats.norm.sf(z), tail))
        F = d1.var(ddof=1) / d3.var(ddof=1)
        dfa, dfb = d1.size - 1, d3.size - 1
        put(f"test.f2s.{tail}.F", F); put(f"test.f2s.{tail}.p", ptail(stats.f.cdf(F, dfa, dfb), stats.f.sf(F, dfa, dfb), tail))
        x, y = arr("ds2_x"), arr("ds4_y")
        lr = stats.linregress(x, y, alternative=ALT[tail])
        put(f"test.treg.{tail}.t", lr.slope / lr.stderr); put(f"test.treg.{tail}.p", lr.pvalue)
    x, y = arr("ds2_x"), arr("ds4_y")
    lr = stats.linregress(x, y)
    put("test.treg.a", lr.intercept); put("test.treg.b", lr.slope); put("test.treg.r", lr.rvalue)
    put("test.treg.s", math.sqrt(((y - lr.intercept - lr.slope * x) ** 2).sum() / (x.size - 2)))
    put("test.t2s.1.sp", math.sqrt(((d1.size - 1) * d1.var(ddof=1) + (d3.size - 1) * d3.var(ddof=1)) / (d1.size + d3.size - 2)))
    o, e = arr("gof_o"), arr("gof_e")
    cs = stats.chisquare(o, e)
    put("test.gof.chi", cs.statistic); put("test.gof.p", cs.pvalue)
    for i, c in enumerate((o - e) ** 2 / e):
        put(f"test.gof.cntrb{i}", c)
    obs = arr("way2_o").reshape(2, 3)
    chi, p, dof, exp = stats.chi2_contingency(obs, correction=False)
    put("test.way2.chi", chi); put("test.way2.p", p); put("test.way2.df", dof)
    for i, v in enumerate(exp.flatten()):
        put(f"test.way2.e{i}", v)
    # intervalli al 95 %
    z = stats.norm.ppf(0.975)
    h = z * 2.5 / math.sqrt(d1.size)
    put("int.z1s.left", d1.mean() - h); put("int.z1s.right", d1.mean() + h)
    h = z * math.sqrt(2.5 ** 2 / d1.size + 1.2 ** 2 / d3.size)
    put("int.z2s.left", d1.mean() - d3.mean() - h); put("int.z2s.right", d1.mean() - d3.mean() + h)
    ph = 55 / 100
    h = z * math.sqrt(ph * (1 - ph) / 100)
    put("int.z1p.left", ph - h); put("int.z1p.right", ph + h)
    p1, p2 = 45 / 100, 60 / 120
    h = z * math.sqrt(p1 * (1 - p1) / 100 + p2 * (1 - p2) / 120)
    put("int.z2p.left", p1 - p2 - h); put("int.z2p.right", p1 - p2 + h)
    lo, hi = stats.t.interval(0.95, d1.size - 1, loc=d1.mean(), scale=d1.std(ddof=1) / math.sqrt(d1.size))
    put("int.t1s.left", lo); put("int.t1s.right", hi)
    for pooled in (True, False):
        ci = stats.ttest_ind(d1, d3, equal_var=pooled).confidence_interval(0.95)
        put(f"int.t2s.{int(pooled)}.left", ci.low); put(f"int.t2s.{int(pooled)}.right", ci.high)
    # ANOVA: una via (scipy) e due vie (statsmodels, disegno bilanciato)
    a, yv = arr("an1_a"), arr("an1_y")
    groups = [yv[a == g] for g in np.unique(a)]
    fo = stats.f_oneway(*groups)
    grand = yv.mean()
    ssa = sum(g.size * (g.mean() - grand) ** 2 for g in groups)
    sse = sum(((g - g.mean()) ** 2).sum() for g in groups)
    put("anova1.ssa", ssa); put("anova1.sse", sse); put("anova1.f", fo.statistic); put("anova1.p", fo.pvalue)
    for name, rep in (("anova2", "an2"), ("anova3", "an3")):
        df = pd.DataFrame({"A": arr(rep + "_a"), "B": arr(rep + "_b"), "y": arr(rep + "_y")})
        formula = "y ~ C(A) * C(B)" if name == "anova2" else "y ~ C(A) + C(B)"
        table = sm.stats.anova_lm(ols(formula, data=df).fit(), typ=2)
        rows = {"C(A)": "a", "C(B)": "b", "C(A):C(B)": "ab", "Residual": "e"}
        for idx, key in rows.items():
            if idx not in table.index:
                continue
            put(f"{name}.ss{key}", table.loc[idx, "sum_sq"])
            put(f"{name}.df{key}", table.loc[idx, "df"])
            if key != "e":
                put(f"{name}.f{key}", table.loc[idx, "F"]); put(f"{name}.p{key}", table.loc[idx, "PR(>F)"])


# ---------------------------------------------------------------- distribuzioni
def distributions():
    put("dist.npd", stats.norm.pdf(1.3, loc=0.5, scale=1.7))
    put("dist.ncd", stats.norm.cdf(2.1, 0.5, 1.7) - stats.norm.cdf(-0.4, 0.5, 1.7))
    put("dist.invn.left", stats.norm.ppf(0.3, 0.5, 1.7))
    put("dist.invn.right", stats.norm.isf(0.3, 0.5, 1.7))
    lo, hi = stats.norm.interval(0.8, 0.5, 1.7)
    put("dist.invn.c1", lo); put("dist.invn.c2", hi)
    put("dist.tpd", stats.t.pdf(0.7, 6))
    put("dist.tcd", stats.t.cdf(1.9, 6) - stats.t.cdf(-0.8, 6))
    put("dist.invt", stats.t.isf(0.025, 6))
    put("dist.cpd", stats.chi2.pdf(2.5, 4))
    put("dist.ccd", stats.chi2.cdf(5.2, 4) - stats.chi2.cdf(1.1, 4))
    put("dist.invc", stats.chi2.isf(0.05, 4))
    put("dist.fpd", stats.f.pdf(1.4, 4, 9))
    put("dist.fcd", stats.f.cdf(3.0, 4, 9) - stats.f.cdf(0.5, 4, 9))
    put("dist.invf", stats.f.isf(0.05, 4, 9))
    put("dist.bpd", stats.binom.pmf(7, 20, 0.35))
    put("dist.bcd", stats.binom.cdf(9, 20, 0.35) - stats.binom.cdf(3, 20, 0.35))
    put("dist.invb", stats.binom.ppf(0.6, 20, 0.35))
    put("dist.ppd", stats.poisson.pmf(4, 3.2))
    put("dist.pcd", stats.poisson.cdf(6, 3.2) - stats.poisson.cdf(1, 3.2))
    put("dist.invp", stats.poisson.ppf(0.75, 3.2))
    put("dist.gpd", stats.geom.pmf(3, 0.3))
    put("dist.gcd", stats.geom.cdf(5, 0.3) - stats.geom.cdf(1, 0.3))
    put("dist.invg", stats.geom.ppf(0.8, 0.3))
    # fx-CG50: HypergeoPD(x, n, M, N) = n estrazioni, M successi, N popolazione; scipy: (M=N, n=M, N=n)
    put("dist.hpd", stats.hypergeom.pmf(2, 25, 8, 6))
    put("dist.hcd", stats.hypergeom.cdf(3, 25, 8, 6) - stats.hypergeom.cdf(0, 25, 8, 6))
    put("dist.invh", stats.hypergeom.ppf(0.7, 25, 8, 6))
    # lista (Npd, Bcd con U.List e Lower 0, InvT)
    for i, x in enumerate((-1.0, 0.0, 0.5, 2.0, 3.5)):
        put(f"dist.list.npd{i}", stats.norm.pdf(x, 0.5, 1.7))
    for i, u in enumerate((0, 3, 7, 12, 20)):
        put(f"dist.list.bcd{i}", stats.binom.cdf(u, 20, 0.35))
    for i, a in enumerate((0.01, 0.05, 0.1, 0.5, 0.9)):
        put(f"dist.list.invt{i}", stats.t.isf(a, 6))
    # grafico N-Dist: picco della curva con x̄ e σx dei dati
    d1 = arr("ds1_x")
    put("graph.ndist.peak", stats.norm.pdf(d1.mean(), d1.mean(), d1.std()))
    xs = np.sort(d1)
    zs = stats.norm.ppf((np.arange(1, xs.size + 1) - 0.5) / xs.size)
    for i, z in enumerate(zs):
        put(f"graph.npp.z{i}", z)


def generate():
    REFS.clear()
    for name, (x, f) in {"desc.ds1": ("ds1_x", None), "desc.ds1f": ("ds1_x", "ds1_f"), "desc.qa": ("q_a", None),
                         "desc.qb": ("q_b", None), "desc.qc": ("q_c", None), "desc.qd": ("q_d", None), "desc.qe": ("q_e", None),
                         "desc.qef": ("q_e", "q_e_f"), "desc.qg": ("q_g", None), "desc.ds3": ("ds3_x", None)}.items():
        one_var(name, DATA[x], DATA[f] if f else None)
    two_var("desc2.ds2", DATA["ds2_x"], DATA["ds2_y"])
    two_var("desc2.ds2f", DATA["ds2_x"], DATA["ds2_y"], DATA["ds2_f"])
    for suffix, f in (("", None), ("f", DATA["ds2_f"])):
        x, y, y4 = DATA["ds2_x"], DATA["ds2_y"], DATA["ds4_y"]
        reg_linear("reg.ax" + suffix, x, y, f)
        reg_linear("reg.a+bx" + suffix, x, y, f, swap=True)
        medmed("reg.med" + suffix, x, y, f)
        reg_poly("reg.quad" + suffix, x, y4, 2, f)
        reg_poly("reg.cubic" + suffix, x, y4, 3, f)
        reg_poly("reg.quart" + suffix, x, y4, 4, f)
        reg_transformed("reg.log" + suffix, x, y, "log", f)
        reg_transformed("reg.exp" + suffix, x, y4, "exp", f)
        reg_transformed("reg.abexp" + suffix, x, y4, "abexp", f)
        reg_transformed("reg.power" + suffix, x, y4, "power", f)
    reg_sin("regit.sin")
    reg_logistic("regit.lgs")
    tests()
    distributions()
    lines = ["// Generato da tools/stat_reference.py: NON modificare a mano.",
             f"// scipy {scipy.__version__}, numpy {np.__version__}, statsmodels {statsmodels.__version__}, pandas {pd.__version__}.",
             "// Dati fissi e valori attesi (chiave, valore) per tests/statistics_test.cpp."]
    for name, values in DATA.items():
        body = ", ".join("%.17g" % float(v) for v in values)
        lines.append(f"const std::vector<double> {name} = {{{body}}};")
    lines.append("const RefValue kRefs[] = {")
    for key, value in REFS:
        lines.append(f'    {{"{key}", {"%.17g" % value}}},')
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    text = generate()
    if "--check" in sys.argv:
        saved = OUT.read_text() if OUT.exists() else ""
        if saved != text:
            print(f"stat_reference: {OUT} non corrisponde ai riferimenti rigenerati", file=sys.stderr)
            return 1
        print(f"stat_reference: {len(REFS)} valori identici a {OUT.relative_to(ROOT)}")
        return 0
    OUT.write_text(text)
    print(f"stat_reference: {len(REFS)} valori scritti in {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
