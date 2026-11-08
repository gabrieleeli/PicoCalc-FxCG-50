# PicoCalc FX AI - motore matematico locale (MicroPython friendly)
import math

def solve_quadratic(a, b, c, eps=1e-12):
    a, b, c = float(a), float(b), float(c)
    if abs(a) < eps:
        if abs(b) < eps:
            return {"kind": "identity" if abs(c) < eps else "impossible"}
        return {"kind": "linear", "root": -c / b}
    delta = b*b - 4*a*c
    if delta > eps:
        s = math.sqrt(delta)
        # Formula stabile numericamente
        q = -0.5 * (b + (s if b >= 0 else -s))
        if abs(q) < eps:
            x1 = (-b + s)/(2*a)
            x2 = (-b - s)/(2*a)
        else:
            x1, x2 = q/a, c/q
        return {"kind": "two_real", "delta": delta, "roots": tuple(sorted((x1, x2)))}
    if abs(delta) <= eps:
        return {"kind": "double", "delta": 0.0, "root": -b/(2*a)}
    real = -b/(2*a)
    imag = math.sqrt(-delta)/(2*abs(a))
    return {"kind": "complex", "delta": delta, "root": (real, imag)}

def solve_linear_system(A, b, eps=1e-10):
    """Risolutore Gauss con pivot parziale. Restituisce soluzione o classificazione."""
    n = len(A)
    if n == 0 or len(b) != n or any(len(row) != n for row in A):
        raise ValueError("Il sistema deve essere quadrato (n x n).")
    m = [[float(A[i][j]) for j in range(n)] + [float(b[i])] for i in range(n)]
    rank = 0
    for col in range(n):
        if rank >= n:
            break
        pivot = max(range(rank, n), key=lambda r: abs(m[r][col]))
        if abs(m[pivot][col]) < eps:
            continue
        m[rank], m[pivot] = m[pivot], m[rank]
        p = m[rank][col]
        for j in range(col, n+1):
            m[rank][j] /= p
        for r in range(n):
            if r != rank:
                f = m[r][col]
                for j in range(col, n+1):
                    m[r][j] -= f*m[rank][j]
        rank += 1
    for row in m:
        if all(abs(row[j]) < eps for j in range(n)) and abs(row[n]) >= eps:
            return {"kind": "inconsistent"}
    if rank < n:
        return {"kind": "dependent", "rank": rank}
    return {"kind": "unique", "solution": [m[i][n] for i in range(n)]}

def _quad_roots(a, b, c, eps=1e-12):
    if abs(a) < eps:
        if abs(b) < eps:
            return []
        return [-c/b]
    d = b*b - 4*a*c
    if d < -eps:
        return []
    if abs(d) <= eps:
        return [-b/(2*a)]
    s = math.sqrt(d)
    return sorted([(-b-s)/(2*a), (-b+s)/(2*a)])

def solve_quadratic_inequality(a, b, c, op):
    """Restituisce intervalli approssimati come tuple (estremo, incluso, estremo, incluso)."""
    if op not in ("<", "<=", ">", ">="):
        raise ValueError("Operatore non valido.")
    a, b, c = float(a), float(b), float(c)
    eps = 1e-12
    if abs(a) < eps:
        if abs(b) < eps:
            ok = {"<": c < 0, "<=": c <= 0, ">": c > 0, ">=": c >= 0}[op]
            return {"kind": "all" if ok else "empty", "intervals": [(-math.inf, False, math.inf, False)] if ok else []}
        r = -c/b
        # b*x+c op 0
        positive_right = b > 0
        want_positive = op in (">", ">=")
        right_side = positive_right == want_positive
        inc = op in ("<=", ">=")
        ints = [(r, inc, math.inf, False)] if right_side else [(-math.inf, False, r, inc)]
        return {"kind": "intervals", "intervals": ints, "roots": [r]}
    roots = _quad_roots(a,b,c)
    inc = op in ("<=", ">=")
    want_positive = op in (">", ">=")
    if not roots:
        val = c
        ok = (val > 0 if want_positive else val < 0)
        if a < 0: ok = not ok
        return {"kind": "all" if ok else "empty", "intervals": [(-math.inf,False,math.inf,False)] if ok else []}
    if len(roots) == 1:
        r = roots[0]
        # A parabola touches zero and keeps the sign of a
        positive = a > 0
        ok_away = positive == want_positive
        intervals = []
        if ok_away:
            intervals = [(-math.inf,False,r,inc),(r,inc,math.inf,False)]
        elif inc:
            intervals = [(r,True,r,True)]
        return {"kind":"intervals","intervals":intervals,"roots":roots}
    r1, r2 = roots
    positive_outside = a > 0
    want_outside = positive_outside == want_positive
    if want_outside:
        intervals = [(-math.inf,False,r1,inc),(r2,inc,math.inf,False)]
    else:
        intervals = [(r1,inc,r2,inc)]
    return {"kind":"intervals","intervals":intervals,"roots":roots}

def _safe_function(expr):
    """Valuta espressioni numeriche con soli nomi matematici consentiti."""
    allowed = {
        "x": 0.0, "pi": math.pi, "e": math.e,
        "sin": math.sin, "cos": math.cos, "tan": math.tan,
        "asin": math.asin, "acos": math.acos, "atan": math.atan,
        "sqrt": math.sqrt, "log": math.log, "log10": math.log10,
        "exp": math.exp, "abs": abs
    }
    # Allowlist sintattica: impedisce accesso ad attributi, import e builtins.
    allowed_names = ("x", "pi", "e", "sin", "cos", "tan", "asin", "acos",
                     "atan", "sqrt", "log", "log10", "exp", "abs")
    token = ""
    for ch in expr:
        if ch.isalnum() or ch == "_":
            token += ch
        else:
            if token:
                if token not in allowed_names and not token.isdigit():
                    raise ValueError("Nome non consentito: " + token)
                token = ""
            if ch not in " +-*/().,%":
                raise ValueError("Carattere non consentito nell'espressione.")
    if token and token not in allowed_names and not token.isdigit():
        raise ValueError("Nome non consentito: " + token)
    if "__" in expr or "[" in expr or "]" in expr or "'" in expr or '"' in expr:
        raise ValueError("Sintassi non consentita.")
    code = compile(expr, "<funzione>", "eval")
    def f(x):
        env = dict(allowed)
        env["x"] = x
        return eval(code, {"__builtins__": {}}, env)
    return f

def analyze_parity(expr, samples=40, tol=1e-7):
    f = _safe_function(expr)
    even_ok, odd_ok, valid = True, True, 0
    for i in range(1, samples+1):
        x = 0.13 + i*0.217
        try:
            p, n = f(x), f(-x)
            if not (math.isfinite(p) and math.isfinite(n)):
                continue
            valid += 1
            scale = max(1.0, abs(p), abs(n))
            if abs(n-p) > tol*scale: even_ok = False
            if abs(n+p) > tol*scale: odd_ok = False
        except Exception:
            continue
    if valid < samples//3:
        return {"kind":"undetermined", "note":"Troppi punti fuori dal dominio."}
    if even_ok: return {"kind":"even", "note":"Verifica numerica su campioni simmetrici."}
    if odd_ok: return {"kind":"odd", "note":"Verifica numerica su campioni simmetrici."}
    return {"kind":"neither", "note":"Non risulta né pari né dispari nei campioni."}

def check_period(expr, period, samples=40, tol=1e-6):
    if period <= 0:
        raise ValueError("Il periodo candidato deve essere positivo.")
    f = _safe_function(expr)
    valid = 0
    for i in range(samples):
        x = -3.7 + i*0.193
        try:
            a, b = f(x), f(x+period)
            if not (math.isfinite(a) and math.isfinite(b)):
                continue
            valid += 1
            if abs(a-b) > tol*max(1.0,abs(a),abs(b)):
                return {"kind":"not_confirmed", "valid":valid}
        except Exception:
            continue
    if valid < samples//2:
        return {"kind":"undetermined", "valid":valid}
    return {"kind":"candidate", "valid":valid}
